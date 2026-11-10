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

    /** \name Level editing (used by the level editor; see also Physics) \{ */
    std::map<ELevel, std::vector<SceneObjectPhysicsParameters>> &getSceneObjects()
    {
        return mPhysics.sceneObjects;
    }
    SceneObjectPhysicsParameters &addSceneObject(ELevel level,
                                                 SceneObjectPhysicsParameters object)
    {
        return mPhysics.addSceneObject(level, std::move(object));
    }
    bool removeSceneObject(b2BodyId bodyId)
    {
        return mPhysics.removeSceneObject(bodyId);
    }
    /** Re-creates all scene-object bodies and nodes of \p level from the
        current level data (used after edits or when switching levels). */
    void reloadSceneObjects(ELevel level)
    {
        mPhysics.reloadSceneObjects(level);
    }
    /** The level whose scene-object bodies are currently created. */
    ELevel getActiveLevel() { return mPhysics.getActiveLevel(); }
    /** Moves a scene object live during a drag; see Physics::moveSceneObject. */
    bool moveSceneObject(b2BodyId bodyId, const b2Vec2& position)
    {
        return mPhysics.moveSceneObject(bodyId, position);
    }
    /** Rotates a scene object live around a world-space pivot; see
        Physics::rotateSceneObject. */
    bool rotateSceneObject(b2BodyId bodyId, float angleRadians,
                           const b2Vec2& pivotM)
    {
        return mPhysics.rotateSceneObject(bodyId, angleRadians, pivotM);
    }
    /** Current rotation (radians) of a scene-object body; 0 if unknown. */
    float getSceneObjectRotation(b2BodyId bodyId) const
    {
        return mPhysics.getSceneObjectRotation(bodyId);
    }
    /** Recreates the body of an edited scene object; see Physics. */
    b2BodyId updateSceneObject(b2BodyId bodyId)
    {
        return mPhysics.recreateSceneObject(bodyId);
    }
    /** Scene object lookup by body id; nullptr if unknown. */
    SceneObjectPhysicsParameters* findSceneObject(b2BodyId bodyId)
    {
        return mPhysics.findSceneObject(bodyId);
    }
    /** Hit-test the scene objects of \p level at a world position (meters). */
    b2BodyId getSceneObjectAt(ELevel level, const b2Vec2& position)
    {
        return mRenderer->getSceneObjectAt(level, position);
    }
    /** Hover/selection/preview overlays, see Renderer. */
    void setHoverHighlight(b2BodyId bodyId, bool withOutline = false)
    {
        mRenderer->setHoverHighlight(bodyId, withOutline);
    }
    void setSelectedHighlight(b2BodyId bodyId)
    {
        mRenderer->setSelectedHighlight(bodyId);
    }
    /// marker style of the selected object: resize handles or rotate arrows
    void setSelectionStyle(Renderer::SelectionStyle style)
    {
        mRenderer->setSelectionStyle(style);
    }
    /// pivot-point marker of the selected object (rotate tool)
    void setPivotMarker(const sf::Vector2f& positionPx, bool show = true)
    {
        mRenderer->setPivotMarker(positionPx, show);
    }
    void setResizePreview(const sf::Vector2f& positionPx,
                          const sf::Vector2f& sizePx, bool show = true,
                          float angleRad = 0.f)
    {
        mRenderer->setResizePreview(positionPx, sizePx, show, angleRad);
    }
    void setGridOverlay(bool show, const sf::Vector2f& cellSizePx,
                        const sf::Color& color)
    {
        mRenderer->setGridOverlay(show, cellSizePx, color);
    }
    void setPlayerOnTop(bool onTop)
    {
        mRenderer->setPlayerOnTop(onTop);
    }
    /// level-editor selection mode: pauses the physics world stepping (the
    /// playable character keeps its pose; drags teleport bodies exactly)
    void setPhysicsActive(bool active) { mPhysicsActive = active; }
    /// MAIN CHARACTER (level editor): rect of its physics body (center in
    /// world meters + shape extents in px, duck/stand aware) ...
    Physics::CharacterPhysicsRect getCharacterRect()
    {
        return mPhysics.getCharacterRect(ECharacters::Ichi);
    }
    void moveMainCharacterToPx(const sf::Vector2f& positionPx)
    {
        mPhysics.setCharacterPositionPx(ECharacters::Ichi, positionPx);
    }
    void setCharacterSelection(const sf::Vector2f& centerPx,
                               const sf::Vector2f& sizePx, bool show = true)
    {
        mRenderer->setCharacterSelection(centerPx, sizePx, show);
    }
    /** \} */

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
    /// physics world-stepping gate (level editor selection mode)
    bool mPhysicsActive = true;
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
