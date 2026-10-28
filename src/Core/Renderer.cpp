#include "Ryu/Core/Renderer.h"
#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Events/EventEnums.h"
#include "Ryu/Events/EventBus.h"
#include "Ryu/Core/Utilities.h"
#include "Ryu/Scene/Entity.h"
#include "Ryu/Scene/SceneEnums.h"
#include "Ryu/Core/EventManager.h"
#include "Ryu/Character/CharacterIchi.h"
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/RectangleShape.hpp>

Renderer::Renderer(sf::RenderWindow& window, EventManager& eventManager, const std::map<ELevel, std::vector<SceneObjectPhysicsParameters>>& sceneObjects)
     : mPhysicsAssetsManager()
     , mWindow(window)
     , mSceneAssetsManager()
     , mCharacterAssetsManager()
     , mSceneGraph()
     , mWorldBounds(
          {0.f, 0.f},
          {640.f, 360.f})  // Use fixed canvas resolution for world bounds
     , mSpawnPosition(mWorldBounds.size.x / 2.f,
              mWorldBounds.size.y - 50)  // Spawn near bottom (50 units from bottom)
     , mPushBox(nullptr)
     , mActiveCommands()
     , mSceneObjects(sceneObjects)
     , mEventManager(eventManager)
 {
     // TODO: add assets to assetmanager like
     // baseTextureManager.load(Textures::PhysicAssetsID::Empty, "assets/scenes/99_dummy/box_empty.png");
     loadTextures();
     buildScene();
     // TODO: more generic scenobject - creation
     createSceneFromConfiguration(ELevel::Level2);

     // Subscribe to physics events
     EventBus::subscribe(Ryu::EPhysicsEvent::ObjectCreated
                         , [this](std::any data)
                         {
                             auto event = std::any_cast<PhysicsObjectCreatedEvent>(data);
                             this->onPhysicsObjectCreated(event);
                         });

     EventBus::subscribe(Ryu::EPhysicsEvent::ObjectUpdated
                         , [this](std::any data)
                         {
                             auto event = std::any_cast<PhysicsObjectUpdatedEvent>(data);
                             this->onPhysicsObjectUpdated(event);
                         });


     EventBus::subscribe(Ryu::EPhysicsEvent::ObjectDestroyed
         , [this](std::any data)
         {
             auto event = std::any_cast<PhysicsObjectDestroyedEvent>(data);
             this->onPhysicsObjectDestroyed(event);
         });

}

void Renderer::loadTextures()
{
    mPhysicsAssetsManager.load(Textures::PhysicAssetsID::Crate,
                            "assets/scenes/99_dummy/box_wood.png");
    
    mSceneAssetsManager.load(Textures::SceneID::BoxPushable,
                        "assets/scenes/99_dummy/box_wood.png");
    mSceneAssetsManager.load(Textures::SceneID::BGMountain,
                        "assets/backgrounds/99_dummy/722756.png");
    mSceneAssetsManager.load(Textures::SceneID::Grass,
                        "assets/scenes/99_dummy/tile_grass_1.png");
    mSceneAssetsManager.load(Textures::SceneID::Button,
                        "assets/scenes/99_dummy/tile_button_1.png");
    mSceneAssetsManager.load(Textures::SceneID::Teleport,
                        "assets/scenes/99_dummy/tile_teleport_1.png");
    mSceneAssetsManager.load(Textures::SceneID::Grate,
                        "assets/scenes/99_dummy/tile_grate_1.png");

}

void Renderer::buildScene() {
    // set Layer
    for (std::size_t i = 0; i < size_t(Layer::LayerCount); ++i) {
        std::shared_ptr<SceneNode> layer = std::make_shared<SceneNode>();
        mSceneLayers[i] = layer;
        mSceneGraph.attachChild(std::move(layer));
    }
    
    // TODO: background is also scene-dependent
    // move to where all the sceneobjects of a scene will be located
    sf::Texture &textureBg =
        mSceneAssetsManager.getResource(Textures::SceneID::BGMountain);
    sf::IntRect textureRect(sf::Vector2i(0, 0), sf::Vector2i(static_cast<int>(mWorldBounds.size.x), static_cast<int>(mWorldBounds.size.y)));

    std::unique_ptr<SpriteNode> backgroundSprite =
        std::make_unique<SpriteNode>(textureBg, textureRect);
    backgroundSprite->setPosition({mWorldBounds.position.x, mWorldBounds.position.y});
    mSceneLayers[static_cast<unsigned>(Layer::Background)]->attachChild(
        std::move(backgroundSprite));

    // pushable Box / moving platform test
    std::unique_ptr<Box> box =
        std::make_unique<Box>(Box::Type::Pushable, mSceneAssetsManager);
    mPushBox = box.get();
    mPushBox->setPosition(sf::Vector2f(760.f,80.f));
    
    mSceneLayers[static_cast<size_t>(Layer::Ground1)]->attachChild(
        std::move(box));

    auto player = mEventManager.requestPlayer();

    if (player) {
    mSceneLayers[static_cast<size_t>(Layer::Ground1)]->attachChild(
        std::static_pointer_cast<SceneNode>(player));
    } else {
        std::cerr << "WARNING: Player is null in Renderer::buildScene()" << std::endl;
    }
}

void Renderer::draw()
{
    mWindow.draw(mSceneGraph);
}

void Renderer::update(sf::Time dt)
{
    while (!mActiveCommands.isEmpty()) {
        mSceneGraph.onCommand(mActiveCommands.pop(), dt);
    }
    mSceneGraph.update(dt);
}

CommandQueue& Renderer::getActiveCommands()
{
    return mActiveCommands;
}


void
Renderer::onPhysicsObjectCreated(const PhysicsObjectCreatedEvent& event)
{
    // Create a SpriteNode for the physics object
    sf::Texture* texture = nullptr;
    
    if (auto* physicsTextureId = std::get_if<Textures::PhysicAssetsID>(&event.textureId))
    {
        if (*physicsTextureId != Textures::PhysicAssetsID::Empty)
        {
            texture = &mPhysicsAssetsManager.getResource(*physicsTextureId);
        }
        else
        {
            texture = &mPhysicsAssetsManager.getResource(Textures::PhysicAssetsID::Empty);
        }
    }
    else if (auto* sceneTextureId = std::get_if<Textures::SceneID>(&event.textureId))
    {
        texture = &mSceneAssetsManager.getResource(*sceneTextureId);
    }
    else if(auto* spriteSheetTextureId = std::get_if<Textures::SpritesheetID>(&event.textureId))
    {
        texture = &mCharacterAssetsManager.getResource(*spriteSheetTextureId);
    }

    if (texture)
    {
        // Convert size from meters (Box2D) to pixels (SFML)
        sf::Vector2f sizePixels{
            Converter::metersToPixels(event.size.x),
            Converter::metersToPixels(event.size.y)
        };
        
        auto spriteNode = std::make_unique<SpriteNode>(*texture);
        
        // Scale the sprite to match the physics object size
        sf::Vector2f textureSize(static_cast<float>(texture->getSize().x), 
                                static_cast<float>(texture->getSize().y));
        sf::Vector2f scale(sizePixels.x / textureSize.x, sizePixels.y / textureSize.y);
        spriteNode->setScale(scale);
        
        // Set origin to center for proper positioning
        spriteNode->setOrigin(textureSize / 2.0f);
        
        // Convert position from meters (Box2D) to pixels (SFML)
        spriteNode->setPosition(sf::Vector2f{
            Converter::metersToPixels(event.position.x),
            Converter::metersToPixels(event.position.y)
        });
        
        // Attach to the appropriate layer and get the shared_ptr
        // For physics objects created via events, use Ground1 layer by default
        // TODO: check if this is valid for all cases, probably not
        auto nodeShared = mSceneLayers[static_cast<size_t>(Layer::Ground1)]->attachChildReturningPtr(std::move(spriteNode));
        
        // Store the mapping from bodyId to the sprite node for updates
        mPhysicsBodyToNode[event.bodyId] = nodeShared;
    }
}

void
Renderer::onPhysicsObjectUpdated(const PhysicsObjectUpdatedEvent& event)
{
    // Find the sprite node for this physics body
    auto it = mPhysicsBodyToNode.find(event.bodyId);
    if (it != mPhysicsBodyToNode.end() && !it->second.expired())
    {
        auto node = it->second.lock();
        if (node)
        {
            // Convert position from meters (Box2D) to pixels (SFML)
            node->setPosition(sf::Vector2f{
                Converter::metersToPixels(event.position.x),
                Converter::metersToPixels(event.position.y)
            });
            node->setRotation(sf::radians(event.rotation));
        }
    }
}

void
Renderer::onPhysicsObjectDestroyed(const PhysicsObjectDestroyedEvent& event)
{
    // Remove the destroyed physics object from the mapping
    mPhysicsBodyToNode.erase(event.bodyId);
    
    // Note: With the scenegraph approach, we don't need to manually remove objects
    // as they should be managed by the scenegraph.
}

void Renderer::createSceneFromConfiguration(ELevel level)
{
    for (auto& obj : mSceneObjects.at(level))
    {
        const sf::Texture& texture = mSceneAssetsManager.getResource(obj.mTextureId);
        
        // Convert size from meters (Box2D) to pixels (SFML)
        sf::Vector2f sizePixels{
            Converter::metersToPixels(obj.mSize.x),
            Converter::metersToPixels(obj.mSize.y)
        };
        
        // Create SpriteNode with texture and scale it to the desired size
        auto spriteNode = std::make_unique<SpriteNode>(texture);
        
        // Scale the sprite to match the physics object size
        sf::Vector2f textureSize(static_cast<float>(texture.getSize().x), 
                                static_cast<float>(texture.getSize().y));
        sf::Vector2f scale(sizePixels.x / textureSize.x, sizePixels.y / textureSize.y);
        spriteNode->setScale(scale);
        
        // Set origin to center for proper positioning
        spriteNode->setOrigin(textureSize / 2.0f);
        
        // Convert position from meters (Box2D) to pixels (SFML)
        spriteNode->setPosition(sf::Vector2f{
            Converter::metersToPixels(obj.mPosition.x),
            Converter::metersToPixels(obj.mPosition.y)
        });
        
        // Use the layer specified in the scene object configuration
        mSceneLayers[static_cast<size_t>(obj.mLayer)]->attachChild(std::move(spriteNode));
    }
}
