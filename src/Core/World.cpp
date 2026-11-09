#include "Ryu/Core/EventManager.h"
#include "Ryu/Physics/Physics.h"
#include "Ryu/Scene/Entity.h"
#include "Ryu/Scene/LevelManager.h"
#include "Ryu/Scene/SceneEnums.h"
#include "Ryu/Scene/SceneNode.h"
#include <Ryu/Character/CharacterIchi.h>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Core/SpriteNode.h>
#include <Ryu/Core/EventManager.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Core/World.h>
//#include <Ryu/Physics/DebugDraw.h>
#include <Ryu/Physics/Raycast.h>
#include <Ryu/Scene/Crate.h>
#include <Ryu/Scene/EntityStatic.h>

#include <Ryu/Debug/b2DrawSFML.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>

#include <SFML/Graphics/Shape.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Vector2.hpp>
#include <box2d/box2d.h>
// TODO: integrate the files in the project
#include <../build_arch/_deps/tracy-src/public/tracy/Tracy.hpp>

#include <array>
#include <iostream>
#include <memory>
#include <tracy/Tracy.hpp>
#include <utility>

// namespace ryu{
World::World(sf::RenderWindow &window, EventManager& eventManager)
    : Observer("World"), mWindow(window), mWorldView(window.getDefaultView()),
      mWorldBounds(
          {0.f, 0.f},
          {640.f, 360.f}),  // Use fixed canvas resolution for world bounds
      mSpawnPosition({mWorldBounds.size.x / 2.f,
              mWorldBounds.size.y / 2.f}),  // Spawn in center of world
      mPhysics(),
      phDebugPhysics(false), clock(),
      levelManager(std::make_unique<LevelManager>()),
      mEventManager(eventManager)
 {

    mRenderer = std::make_unique<Renderer>(mWindow, mEventManager, mPhysics.sceneObjects);

    // TODO: in ctor we only should do 1 thing and if the following depends on this
    // only do this when the former is finished -> or/and PRO: use multithreading
    // BUT see flow diagram blabla (WIP) SceneWorkflow: after loading textures,set physics
    // use events for this ... we have a nice observer pattern ^^
    loadTextures();

    // Build scene layers BEFORE creating physics objects
    // (Renderer subscribes to physics events in its ctor, so layers must be ready)
    mRenderer->buildScene();

    // build physics - this will emit ObjectCreated events
    //OLD:
    // setPhysics();
    //NEW:
    mPhysics.createPhysicsSceneObjects(ELevel::Level2);
    auto player = mEventManager.requestPlayer();
    mPhysics.initCharacterPhysics(*player, false);
    // connect the character with its physics-owner so state changes
    // (e.g. duck) can rebuild the physics body through Physics
    player->setPhysics(&mPhysics);
    setDebugDrawer(mWindow);

    // TODO: here the scene and the character is created
    // TODO: Race incoming !!!, see Game Ctor (create Ichi)
    // the scene should only be shown when everything is initalized and loaded !!!!
    // st. like: waiting on Event: PhysicsFinished, CharacterFinished, SceneFinished ...

    mWorldView.setCenter(sf::Vector2f{mWorldBounds.size.x / 2.f, mWorldBounds.size.y / 2.f});
}

const sf::Drawable &World::getPlayerSprite() {
    auto player = mEventManager.requestPlayer();
    return player->getSpriteAnimation();
}

void World::loadTextures() {
    // TODO: cant find in debug mode !
    // Textures are now loaded in Renderer
}

void World::onNotify(const SceneNode &entity, Ryu::EEvent event) {
    switch (event._value) {
    case Ryu::EEvent::DebugToggle: {
        toggleDrawDebug();
        break;
    }
    default:
        break;
    }
}

void World::toggleDrawDebug()
{
    phDebugPhysics = not phDebugPhysics;

    if(not phDebugPhysics)
    {
        // mRenderer.reset();
    }
}

void World::setDebugDrawer(sf::RenderTarget &target)
{
    // Initialize the global debug drawer
    debugDrawer.SetTarget(target);
    debugDrawer.SetScale(Converter::PIXELS_PER_METERS);
    debugDrawer.SetAllFlags();

    // Set debug draw for the physics world
    //mPhysics.setDebugDrawer(debugDrawer);
}


// TODO split in "createGroundbodies / setScenePhysics in Physics class"
void World::setPhysics() {

    // TODO: move to physicsclass !

/*
    pBoxTest =
        createPhysicalBox(750, 80, 50, 50, "box_pushable_2", b2_dynamicBody,
                          Textures::SceneID::BoxPushable);
*/
    // sf::Shape* boxShape = getShapeFromPhysicsBody(pBoxTest);
    // newCrate.init(std::move(box),std::move(boxShape));
    // mCrates.push_back(std::move(&newCrate));
}


void World::draw() {

    ZoneScopedN("Draw_World");
    // Use the current window view (which should be the scaled main view)
    // but maintain our camera position
    sf::View currentView = mWindow.getView();
    currentView.setCenter(mWorldView.getCenter());
    mWindow.setView(currentView);
    
    // draw physics
    
    mRenderer->draw();
    
    // TODO: phDebugPhysics && Physics::mDebugPhysicsActive ???
    if (phDebugPhysics)
    {
        // Draw raycasts
        auto player = mEventManager.requestPlayer();
        player->drawRaycasts(debugDrawer);

        b2World_Draw(mPhysics.getPhysicsWorldId(), &debugDrawer.GetDebugDraw());

        // force sfml to restore its texture state
        sf::Texture tempTexture;
        mWindow.draw(sf::Sprite(tempTexture));
    }
    // pBoxTest is deprecated, removed from header

    // Clear window
    //    mWindow.clear();

    /* TODO: how to create texts ? not seem to work here
    for(const auto& text : texts)
    {
        mWindow.draw(text);
    }

    sf::Text text;

    createText("BLUB", text);
    mWindow.draw(text);
    */

    /*
    for(const auto& crate : mCrates)
    {
        mWindow.draw(*(getShapeFromPhysicsBody(crate->getBody())));
    }
    */
}

CommandQueue &World::getActiveCommands() {
    ZoneScopedN("getActCmd_World");

    return mRenderer->getActiveCommands();
}

void World::createText(const sf::String text, sf::Text &textToShow) {
    sf::Font font;
    if (!font.openFromFile("assets/fonts/droid_sans.ttf")) {
        std::cout << "Font Error: font-file not found.\n";
    }

    textToShow.setString(text);

    textToShow.setCharacterSize(24); // in pixels, not points!

    textToShow.setFillColor(sf::Color::Red);

    textToShow.setPosition(sf::Vector2f{100, 100});

    textToShow.setStyle(sf::Text::Bold | sf::Text::Underlined);
}

World::~World() {
    // Physics class handles destruction of bodies in its destructor
}

void World::update(sf::Time dt) {

	ZoneScopedN("Update_World");

	// selection mode pauses the world stepping: dragged bodies keep their
	// pose (no gravity between the drag teleports); the scene graph /
	// character updates below keep running so the visuals still follow
	if (mPhysicsActive)
	{
		mPhysics.update();
	}
    // Draw debug shapes of all physics objects

    // needable or already in scenegraph ?
    // TODO: is this efficient ?
    // we basically use a sharedptr of plyer in update, question is if we need this here
    // TODO: check if the character should be updated over the scenegraph instead
    mEventManager.requestPlayer()->update(dt); //slowmo
    mRenderer->update(dt);

    /*
    for(auto& crate : mCrates)
    {
        std::cout << crate.getBody()->GetPosition().x <<
    crate.getBody()->GetPosition().y << "\n";
    }
    */
}

//} /// namespace ryu
