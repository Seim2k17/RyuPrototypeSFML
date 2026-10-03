#pragma once

#include "Ryu/Core/EventManager.h"
#include "Ryu/Physics/Physics.h"
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/CommandQueue.h>
#include <Ryu/Core/Renderer.h>
#include <Ryu/Debug/b2DrawSFML.hpp>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Events/Observer.h>
#include <Ryu/Scene/Box.h>
#include <Ryu/Scene/Crate.h>
#include <Ryu/Scene/EntityStatic.h>
#include <Ryu/Scene/LevelManager.h>
#include <SFML/Graphics.hpp>

#include <array>
#include <box2d/box2d.h>
#include <vector>
#include <memory>

class CharacterIchi;
class b2World;
class b2Body;
class EventManager;

// namespace ryu {
class World : public Observer {

  public:
    explicit World(sf::RenderWindow &window, EventManager& eventManager);
    ~World();
    void update(sf::Time dt);
    void draw();
    CommandQueue &getActiveCommands();
    const sf::Drawable &getPlayerSprite();
    void toggleDrawDebug();
    void setDebugDrawer(sf::RenderTarget &target);
    void onNotify(const SceneNode &entity, Ryu::EEvent event) override;

  private:
    void createText(const sf::String text, sf::Text &textToShow);
    void loadTextures();
    // TODO: move to physics
    void setPhysics();

    [[deprecated("moved to physics")]]
    b2Body* createPhysicalBox(int pos_x, int pos_y, int size_x, int size_y,
                              std::string name, b2BodyType type,
                              Textures::SceneID texture, EntityType entityType = EntityType::None);
    [[deprecated("moved to physics")]]
    b2Body* createPhysicalBox(/*LevelObject obj*/);

  private:
    sf::RenderWindow &mWindow;
    sf::View mWorldView;
    sf::FloatRect mWorldBounds;
    sf::Vector2f mSpawnPosition;

    std::vector<sf::Text> texts;
    bool phDebugPhysics;
    bool physics_toggled;
    float phTimeStep;

    Physics mPhysics;
    std::unique_ptr<Renderer> mRenderer;

    std::vector<Crate *> mCrates;

    // Clock for calculating delta time (for physics simulation)
    sf::Clock clock;

    std::unique_ptr<LevelManager> levelManager;
    EventManager& mEventManager;
};
//} /// namespace ryu
