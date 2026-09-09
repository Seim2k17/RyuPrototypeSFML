#include "Ryu/Core/Renderer.h"
#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Events/EventEnums.h"
#include "Ryu/Events/EventBus.h"
#include "Ryu/Core/Utilities.h"


 Renderer::Renderer(sf::RenderWindow& window)
     : mPhysicsAssetsManager()
     , mWindow(window)
     , mSceneAssetsManager()
     , mCharacterAssetsManager()
 {
     // TODO: add assets to assetmanager like
     // baseTextureManager.load(Textures::PhysicAssetsID::Empty, "assets/scenes/99_dummy/box_empty.png");
     loadTextures();

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

void Renderer::draw()
{
    for (auto& [name, renderObj] : mRenderObjects)
    {
        if (renderObj.shape)
        {
            mWindow.draw(*renderObj.shape);
        }

    }
}


void
Renderer::onPhysicsObjectCreated(const PhysicsObjectCreatedEvent& event)
{
    auto shape = std::make_unique<sf::RectangleShape>(sf::Vector2f(event.size.x, event.size.y));
    shape->setOrigin({event.size.x / 2.0f, event.size.y / 2.0f});

    if (auto* physicsTextureId = std::get_if<Textures::PhysicAssetsID>(&event.textureId))
    {
        if (*physicsTextureId != Textures::PhysicAssetsID::Empty)
        {
            // TODO: get assets the real way
            shape->setTexture(&mPhysicsAssetsManager.getResource(*physicsTextureId));
        }
        else
        {
            shape->setTexture(&mPhysicsAssetsManager.getResource(Textures::PhysicAssetsID::Empty));
        }
    }
    else if (auto* sceneTextureId = std::get_if<Textures::SceneID>(&event.textureId))
    {
            shape->setTexture(&mSceneAssetsManager.getResource(*sceneTextureId));
        //t.b.c
    }
    else if(auto* spriteSheetTextureId = std::get_if<Textures::SpritesheetID>(&event.textureId))
    {
            shape->setTexture(&mCharacterAssetsManager.getResource(*spriteSheetTextureId));
            // shape->setFillColor(sf::Color::Red); // Red if Spritesheet is Unknown
    }

    mRenderObjects.emplace(event.name, RenderObject{event.bodyId, std::move(shape), event.textureId});
}

void
Renderer::onPhysicsObjectUpdated(const PhysicsObjectUpdatedEvent& event)
{
    auto it = std::find_if(mRenderObjects.begin(), mRenderObjects.end(),
                           [&event](const auto& pair) { return pair.second.bodyId.index1 == event.bodyId.index1; });

    if (it != mRenderObjects.end())
    {
        auto& shape = it->second.shape;
        shape->setPosition({Converter::metersToPixels(event.position.x)
                , Converter::metersToPixels(event.position.y)});
        shape->setRotation(sf::degrees(Converter::radToDeg(event.rotation)));
    }
}

void
Renderer::onPhysicsObjectDestroyed(const PhysicsObjectDestroyedEvent& event)
{
    mRenderObjects.erase(event.name);
}
