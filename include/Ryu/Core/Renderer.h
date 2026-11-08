#pragma once

#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/CommandQueue.h>
#include <Ryu/Events/PhysicsEvents.h>
#include <Ryu/Scene/SceneNode.h>
#include <Ryu/Scene/Box.h>
#include <Ryu/Physics/Physics.h>
#include <Ryu/Core/SpriteNode.h>
#include <SFML/Graphics.hpp>
#include <unordered_map>
#include <array>

using PhysicsAssetsManager = AssetManager<sf::Texture, Textures::PhysicAssetsID>;
using SceneAssetsManager = AssetManager<sf::Texture, Textures::SceneID>;
using CharacterAssetsManager = AssetManager<sf::Texture, Textures::SpritesheetID>;

class EventManager;

class Renderer {
public:
    
    Renderer(sf::RenderWindow& window, EventManager& eventManager, const std::map<ELevel, std::vector<SceneObjectPhysicsParameters>>& sceneObjects);
    
    void draw();
    void buildScene();
    void update(sf::Time dt);
    CommandQueue& getActiveCommands();

    // Layer management
    enum class Layer { Background, Ground1, Foreground, LayerCount };

    /** \name Level-editor helpers (hover / selection / resize overlay) \{ */
    /// Hit-tests the scene objects of \p level against \p position (world
    /// meters); returns the object's bodyId or a null b2BodyId.
    b2BodyId getSceneObjectAt(ELevel level, const b2Vec2& position) const;
    /// Marks the hovered object; a null bodyId clears it. When
    /// \p withOutline is set (selection mode), the hover also gets the red
    /// outline and edge markers.
    void setHoverHighlight(b2BodyId bodyId, bool withOutline = false);
    /// Marks the selected object (drawn with red outline + 8 resize handles);
    /// a null bodyId clears it.
    void setSelectedHighlight(b2BodyId bodyId);
    /// Live-resize preview: an orange overlay drawn at \p positionPx with
    /// \p sizePx while a resize-drag is running.
    void setResizePreview(const sf::Vector2f& positionPx,
                          const sf::Vector2f& sizePx, bool show = true);
    /** \} */

private:
    void loadTextures();

    // Physics assets - section
    PhysicsAssetsManager mPhysicsAssetsManager;
    sf::RenderWindow& mWindow;
    
    // Scene graph and layers
    SceneNode mSceneGraph;
    std::array<std::shared_ptr<SceneNode>, static_cast<size_t>(Layer::LayerCount)> mSceneLayers;
    SceneAssetsManager mSceneAssetsManager;
    sf::FloatRect mWorldBounds;
    sf::Vector2f mSpawnPosition;
    // TODO: just tempoprarly location to store a Box-ptr (just an example)
    Box* mPushBox;
    CommandQueue mActiveCommands;
    EventManager& mEventManager;

    void onPhysicsObjectCreated(const PhysicsObjectCreatedEvent& event);
    void onPhysicsObjectUpdated(const PhysicsObjectUpdatedEvent& event);
    void onPhysicsObjectDestroyed(const PhysicsObjectDestroyedEvent& event);
    // Physics assets - section - end

    // Level editor overlay state (see the helper methods in the public section)
    b2BodyId mHoveredBodyId;
    /// hovered object gets outline + handles too (true while selection mode)
    bool mHoverShowsOutline;
    b2BodyId mSelectedBodyId;
    bool mShowResizePreview;
    sf::Vector2f mResizePreviewPositionPx;
    sf::Vector2f mResizePreviewSizePx;

    void drawLevelEditorOverlay();

    // Character assets
    CharacterAssetsManager mCharacterAssetsManager;
    // Character assets - end

    // Reference to scene configuration from Physics
    const std::map<ELevel, std::vector<SceneObjectPhysicsParameters>>& mSceneObjects;
    void createSceneFromConfiguration(ELevel level);

    // Map to store bodyId -> SpriteNode for physics object updates
    std::unordered_map<b2BodyId, std::weak_ptr<SceneNode>, b2BodyIdHash, b2BodyIdEqual> mPhysicsBodyToNode;
};
