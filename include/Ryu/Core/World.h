#pragma once

#include "Ryu/Physics/Physics.h"
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/CommandQueue.h>
#include <Ryu/Debug/b2DrawSFML.hpp>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Events/Observer.h>
#include <Ryu/Scene/Box.h>
#include <Ryu/Scene/Crate.h>
#include <Ryu/Scene/EntityStatic.h>
#include <Ryu/Scene/LevelManager.h>
#include <Ryu/Scene/SceneNode.h>
#include <SFML/Graphics.hpp>

#include <array>
#include <box2d/b2_math.h>
#include <vector>
#include <memory>

typedef AssetManager<sf::Texture, Textures::SceneID> SceneTextureHolder;

class CharacterIchi;
class b2World;
class b2Body;

static b2DrawSFML debugDrawer;


// namespace ryu {
class World : /*private sf::NonCopyable*/ public Observer {

  public:
    explicit World(sf::RenderWindow &window);
    ~World();
    void update(sf::Time dt);
    void draw();
    CommandQueue &getActiveCommands();
    CharacterIchi *getPlayer();
    const sf::Drawable &getPlayerSprite();
    // TODO: move to physics
    sf::Shape *getShapeFromPhysicsBody(b2Body *physicsBody);
    void debugDrawSegment(b2Vec2 const &p1, b2Vec2 const &p2,
                          b2Color const &color) {
        if (phDebugPhysics) {
            debugDrawer.DrawSegment(p1, p2, color);
        }
    };
    void toggleDrawDebug() { phDebugPhysics = not phDebugPhysics; }
    void setDebugDrawer(sf::RenderTarget &target);
    void onNotify(const SceneNode &entity, Ryu::EEvent event) override;

  private:
    enum class Layer { Background, Ground1, Foreground, LayerCount };

    void createText(const sf::String text, sf::Text &textToShow);
    void loadTextures();
    void buildScene();
    // TODO: move to physics
    void setPhysics();
    b2Body* createPhysicalBox(int pos_x, int pos_y, int size_x, int size_y,
                              std::string name, b2BodyType type,
                              Textures::SceneID texture, EntityType entityType = EntityType::None);
    b2Body* createPhysicalBox(LevelObject obj);

  private:
    sf::RenderWindow &mWindow;
    sf::View mWorldView;
    SceneTextureHolder mSceneTextures;
    SceneNode mSceneGraph;
    std::array<SceneNode *, std::size_t(Layer::LayerCount)> mSceneLayers;
    // Shapes and MetaInfos to static physicalBodies
    std::vector<PhysicsObject> phGroundBodies; // TODO see in Physics -> static sceneObjects
    std::map<uintptr_t, std::unique_ptr<EntityStatic> > mStaticEntities;


    sf::FloatRect mWorldBounds;
    sf::Vector2f mSpawnPosition;
    Box *mPushBox;
    CharacterIchi *mPlayer;

    CommandQueue mActiveCommands;

    Physics mPhysics;

    std::vector<sf::Text> texts;
    // TODO: move to physics
    b2Body* pBoxTest;
    bool phDebugPhysics;
    float phTimeStep;
    // b2DrawSFML debugDrawer;

    std::vector<Crate *> mCrates;

    // Clock for calculating delta time (for physics simulation)
    sf::Clock clock;

    // TODO: does this belongs here ?
    std::unique_ptr<LevelManager> levelManager;
};
//} /// namespace ryu
