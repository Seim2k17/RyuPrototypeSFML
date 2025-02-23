#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Debug/b2DrawSFML.hpp"
#include "Ryu/Scene/EntityStatic.h"
#include "Ryu/Scene/SceneEnums.h"
#include <Ryu/Physics/Physics.h>

#include <Ryu/Core/Utilities.h>

#include <Ryu/Character/ICharacter.h>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Shape.hpp>
#include <SFML/System/Vector2.hpp>
#include <box2d/b2_body.h>
#include <box2d/b2_math.h>
#include <box2d/b2_world.h>
#include <exception>
#include <fmt/core.h>
#include <memory>


const b2Vec2 GRAVITY(0,9.81f);
constexpr float PHYSICS_TIME_STEP = 1.f / 60.f;
constexpr int32 VELOCITY_ITERATIONS = 8;
constexpr int32 POSITION_ITERATIONS = 3;

CharacterPhysicsParameters::CharacterPhysicsParameters() :
    mBody(nullptr),
    mFixture(nullptr),
    mGravityScale(4.8f),  /// for dynamic objects density needs to be > 0
    mFixtureDensity(5.f), /// for dynamic objects density needs to be > 0
    mFixtureFriction(0.1f), /// recommended by  b2d docu
    mFixtureRestitution(0.1f),
    mRaycastLength(40.0f),
    mMoveMultiplier({1.05f, 1.47f}),
    mJumpForwardImpulse({150, -250}),
    mJumpUpImpulse({0, -200}),
    mMassCenter({0, 0}),
    mBodyMass(25)
{}

SceneObjectPhysicsParameters::SceneObjectPhysicsParameters() :
    mPosition({}),
    mSize({}),
    mName(""),
    mType(b2BodyType::b2_staticBody),
    mTextureId(Textures::SceneID::Unknown),
    mEntityType(EntityType::None),
    mPhysicsBody(nullptr) {}
    // polygonShape.SetAsBox(0.5,0.9);


SceneObjectPhysicsParameters::SceneObjectPhysicsParameters(
        sf::Vector2i position, sf::Vector2i size,
        std::string name, b2BodyType type,
        Textures::SceneID textureId, EntityType entityType) :
    mPosition(position),
    mSize(size),
    mName(name),
    mType(type),
    mTextureId(textureId),
    mEntityType(entityType),
    mPhysicsBody(nullptr)
{}

// TODO: set contactlistener (see characterbase)
Physics::Physics() :
    mCharacterPhysics({}),
    mPhysicsWorld(std::make_unique<b2World>(GRAVITY)),
    mStaticEntities(),
    mPhTimeStep(PHYSICS_TIME_STEP),
    mDebugPhysicsActive(false){}

Physics::~Physics()
{
   for(auto& level : sceneObjects)
   {
       for(auto& sceneObj : level.second)
       {
           mPhysicsWorld->DestroyBody(sceneObj.mPhysicsBody);
       }
   }
}

void
Physics::update()
{
    mPhysicsWorld->Step(PHYSICS_TIME_STEP, VELOCITY_ITERATIONS, POSITION_ITERATIONS);
}

void
Physics::setDebugPhysics(bool debugPhysics)
{
    mDebugPhysicsActive = debugPhysics;
}

void
Physics::debugDrawSegment(b2Vec2 const &p1, b2Vec2 const &p2,
                          b2Color const &color) const
{
    if (mDebugPhysicsActive)
    {
        debugDrawer.DrawSegment(p1, p2, color);
    }
}

void
Physics::debugDraw() const
{
    if(mDebugPhysicsActive)
    {
        mPhysicsWorld->DebugDraw();
    }
}

void
Physics::initCharacterPhysics(ICharacter& character, bool inDuckMode)
 {
// TODO: CAUTION:: THERE is an thinking error and a cyclic dependency inside, TOO LATE !!!
   // TODO:
   // - initialize needed values in iniatlizer list of ctor
   // - get implementation from CharacterBase::initPhysics(...)
   // - call accordingly
   //-    // TODO: make it adjustable ? or remove and add new ? -> e.g. duck state ->
    // halfPhysics box
    // init physics after the charactersprite was created !
    // Create the body of the falling Crate
    auto position = character.getPosition();
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody; /// TODO: or even kinematic body ?
    bodyDef.position.Set(Converter::pixelsToMeters<double>(position.x),
                         Converter::pixelsToMeters<double>(
                             inDuckMode
                                 ? position.y + (DUCK_FRAME_SIZE.second / 2)
                                 : position.y));
    bodyDef.fixedRotation = true;
    auto charPhysic = mCharacterPhysics.at(character.getCharacterName());
    bodyDef.gravityScale = charPhysic.mGravityScale;

    // Create a shape
    b2PolygonShape polygonShape;

    const auto shapeSize = inDuckMode ? DUCK_FRAME_SIZE : INIT_FRAME_SIZE;

    int size_x
        = shapeSize
              .first; // mCharacterAnimation.getSprite().getTextureRect().width;
    int size_y = shapeSize.second;

    polygonShape.SetAsBox(Converter::pixelsToMeters<double>(size_x * 0.5f),
                          Converter::pixelsToMeters<double>(size_y * 0.5f));

    // Create a fixture
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &polygonShape;
    fixtureDef.density = charPhysic.mFixtureDensity;
    fixtureDef.friction = charPhysic.mFixtureFriction;
    fixtureDef.restitution = charPhysic.mFixtureRestitution;

    charPhysic.mBody = mPhysicsWorld->CreateBody(&bodyDef);
    charPhysic.mFixture = charPhysic.mBody->CreateFixture(&fixtureDef);

    // TODO: with SFML 3 there are probably smartPointer more elegant ? otherwise do RAII !
    sf::Shape *shape = new sf::RectangleShape(sf::Vector2f(size_x, size_y));
    // or can we delete this/free this at the end of the function ? bc its casted to uintptr_t
    // TODO: test it

    // std::unique_ptr<sf::Shape> shape =
    // std::make_unique<sf::RectangleShape>(sf::Vector2f(size_x, size_y));

    shape->setOrigin({size_x / 2.0f, size_y / 2.0f});
    shape->setPosition(sf::Vector2f(position.x, position.y));
    // TODO check if we need setting a texture
    // shape->setTexture(
    //    &baseTextureManager.getResource(Textures::PhysicAssetsID::Empty));

    charPhysic.mBody->GetUserData().pointer = (uintptr_t)shape; //.get();
    fmt::print("Init character at position {},{}\n",
               Converter::metersToPixels(bodyDef.position.x),
               Converter::metersToPixels(bodyDef.position.y));
    // mBody->SetLinearVelocity(b2Vec2(0.0f, -50.0f));

    // insert physics according to character
    mCharacterPhysics.insert(
        std::make_pair(character.getCharacterName(), charPhysic)
    );
}

void
Physics::createPhysicsSceneObjects(ELevel level)
{
    for(auto& obj : sceneObjects.at(level))
    {
        createPhysicsBody(obj);
        // phGroundBodies.emplace_back(PhysicsObject("", createPhysicalBox(obj)));
    }
}

void
Physics::createPhysicsBody(SceneObjectPhysicsParameters sceneObject)
{
    sf::Vector2i objPosition = sceneObject.mPosition;
    sf::Vector2i objSize = sceneObject.mSize;

    b2BodyDef bodyDef;
    bodyDef.position.Set(Converter::pixelsToMeters<double>(objPosition.x),
                         Converter::pixelsToMeters<double>(objPosition.y));
    bodyDef.type = sceneObject.mType;
    b2PolygonShape b2Shape;

    b2Shape.SetAsBox(Converter::pixelsToMeters<double>(objSize.x / 2.0),
                     Converter::pixelsToMeters<double>(objSize.y / 2.0));

    b2FixtureDef fixtureDef;
    fixtureDef.density = 2.0;
    fixtureDef.friction = 0.98;
    fixtureDef.restitution = 0.1;
    fixtureDef.shape = &b2Shape;

    if(not mPhysicsWorld){
        fmt::print("no physics world created\n");
        return;
    }

    b2Body *res = mPhysicsWorld->CreateBody(&bodyDef);
    // std::unique_ptr<b2Body> res =
    // std::make_unique<b2Body>(phWorld->CreateBody(&bodyDef));
    res->CreateFixture(&fixtureDef);

    // std::unique_ptr<sf::RectangleShape> shape =
    // std::make_unique<sf::RectangleShape>(sf::Vector2f(size_x,size_y));
    // sf::RectangleShape shape{sf::Vector2f(size_x,size_y)};
    // TODO howto smartptr ?
    // sf::Shape *shape = new sf::RectangleShape(objSize);
    std::unique_ptr<sf::Shape> shape =
        std::make_unique<sf::RectangleShape>(sf::Vector2f(objSize));

    shape->setOrigin({(float)(objSize.x / 2.0), (float)(objSize.y / 2.0)});
    shape->setPosition(sf::Vector2f(objPosition.x, objPosition.y));

    // TODO: check what for
    auto staticEntity = std::make_unique<EntityStatic>(sceneObject.mEntityType);
    // std::shared_ptr<EntityStatic> staticEntity =
    // std::make_shared<EntityStatic>());
    staticEntity->setName(sceneObject.mName);

    auto shapePosition = shape->getGlobalBounds().position;
    auto shapeSize = shape->getGlobalBounds().size;
    std::vector<sf::Vector2f> cornerPoints{
        {shapePosition.x, shapePosition.y},
        {shapePosition.x + shapeSize.x, shapePosition.y},
        {shapePosition.x, shapePosition.y + shapeSize.y},
        {shapePosition.x + shapeSize.x,
         shapePosition.y + shapeSize.y}};

    staticEntity->setCornerPoints(cornerPoints);

    if (sceneObject.mTextureId != Textures::SceneID::Unknown) {
         shape->setFillColor(sf::Color::Red);
         shape->setOutlineColor(sf::Color::Red);
         shape->setOutlineThickness(2.0f);
        // TODO: how to set texture without coupling to texturestuff
        // this can be probably be part of the Levelmanager and to LM we need a link
        // from World and from Physics ... ? MAYBE
        // shape->setTexture(&mSceneTextures.getResource(texture));
    }
    else {
        shape->setFillColor(sf::Color::Green);
    }

    staticEntity->setShape(std::move(shape));
    res->GetUserData().pointer = reinterpret_cast<uintptr_t>(staticEntity.get());
    mStaticEntities[reinterpret_cast<uintptr_t>(res)] = std::move(staticEntity);

    // Dangling pointer for EntityStatic ?
    sceneObject.mPhysicsBody = res;

}

void
Physics::setDebugDrawer(b2DrawSFML dbgDrawer)
{
    debugDrawer.SetTarget(dbgDrawer.GetRenderTarget());
    debugDrawer.SetScale(dbgDrawer.GetScale());
    debugDrawer.SetFlags(dbgDrawer.GetFlags());
    mPhysicsWorld->SetDebugDraw(&debugDrawer);// was located as static in World
}
