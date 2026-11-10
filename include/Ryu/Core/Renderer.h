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
    /// Marker style of the selected object in the level editor.
    enum class SelectionStyle {
        ResizeHandles,  /// 8 square handles (corners + edge midpoints)
        RotateMarkers   /// 4 rounded-arrow markers at the corners
    };
    /// Marks the selected object (drawn with red outline; the markers follow
    /// the current selection style); a null bodyId clears it.
    void setSelectedHighlight(b2BodyId bodyId);
    /// Sets the marker style of the selected object (resize vs rotate tool).
    void setSelectionStyle(SelectionStyle style);
    /// Pivot-point marker of the selected object (rotate tool): a small
    /// circle + crosshair drawn at \p positionPx; show=false clears it.
    void setPivotMarker(const sf::Vector2f& positionPx, bool show = true);
    /// Live-resize preview: an orange overlay drawn at \p positionPx with
    /// \p sizePx (rotated by \p angleRad around its center) while a
    /// resize-drag is running.
    void setResizePreview(const sf::Vector2f& positionPx,
                          const sf::Vector2f& sizePx, bool show = true,
                          float angleRad = 0.f);
    /// Grid overlay for the level editor: vertical/horizontal lines over the
    /// whole world bounds, drawn in the foreground (updates live on change)
    void setGridOverlay(bool show, const sf::Vector2f& cellSizePx,
                        const sf::Color& color);
    /// keeps the character visible while the level editor is open: scene
    /// objects are attached after the player (drawn above it), so the player
    /// is re-attached to the Foreground layer while editing
    void setPlayerOnTop(bool onTop);
    /// selection overlay for the main character (level editor): orange fill
    /// + red outline, NO resize markers (the body shape is animation-driven)
    void setCharacterSelection(const sf::Vector2f& centerPx,
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
    /// marker style of the selected object (resize vs rotate tool)
    SelectionStyle mSelectionStyle;
    // pivot-point marker state (see setPivotMarker)
    bool mShowPivotMarker;
    sf::Vector2f mPivotMarkerPositionPx;
    bool mShowResizePreview;
    sf::Vector2f mResizePreviewPositionPx;
    sf::Vector2f mResizePreviewSizePx;
    float mResizePreviewRotationRad;

    // grid overlay state (see setGridOverlay)
    bool mShowGridOverlay;
    sf::Vector2f mGridCellSizePx;
    sf::Color mGridColor;
    /// the player node, needed to move it between layers (editor visibility)
    std::shared_ptr<SceneNode> mPlayerNode;

    // main-character selection overlay state (see setCharacterSelection)
    bool mShowCharacterSelection;
    sf::Vector2f mCharacterSelectionCenterPx;
    sf::Vector2f mCharacterSelectionSizePx;

    void drawLevelEditorOverlay();
    void drawGridOverlay();

    // Character assets
    CharacterAssetsManager mCharacterAssetsManager;
    // Character assets - end

    // Reference to scene configuration from Physics
    const std::map<ELevel, std::vector<SceneObjectPhysicsParameters>>& mSceneObjects;
    void createSceneFromConfiguration(ELevel level);

    // Map to store bodyId -> SpriteNode for physics object updates
    std::unordered_map<b2BodyId, std::weak_ptr<SceneNode>, b2BodyIdHash, b2BodyIdEqual> mPhysicsBodyToNode;
};
