/**
 * ImGui level editor (open with F3).
 *
 * Creates and edits the level data: physics boxes (Box2D) are no longer
 * hardcoded in Physics.h but stored as generic per-level JSON files
 * (assets/levels/levelN.json, see LevelData.h) and created/edited live
 * through the incremental Physics API.
 *
 * Transform modes (mouse in the game view, while this editor is open):
 * - hovering an object highlights it (orange transparent + red outline)
 * - clicking an object selects it and shows 8 resize handles
 *   (corners + edge midpoints); dragging a handle resizes the object live
 * - clicking inside the selected object and dragging moves it live;
 *   releasing fixes the position and leaves the selection mode
 * - left-clicking anywhere else in the game view exits the selection mode
 * Rotate mode (Rotate tool):
 * - the selected object shows 4 rounded-arrow markers at its corners and
 *   its pivot point (circle + crosshair); dragging a corner marker rotates
 *   the object live around the pivot point
 * - dragging the pivot marker moves the pivot, clamped to the object borders
 * While a transform mode is active a red "<mode> active" text is shown at
 * the bottom right of the game view.
 *
 * NOTE: rotation is a live transform on the physics body; it is not (yet)
 * persisted in the level JSON files.
 *
 * @author: Simon Nguyen
 * @date: 02/02/2025
 */

#pragma once

#include <Ryu/Scene/SceneEnums.h>
#include <Ryu/Physics/Physics.h>

#include <box2d/box2d.h>
#include <SFML/System/Vector2.hpp>

#include <string>

namespace sf {
class Event;
}

class World;

namespace RyuLevel
{

class Editor
{
  public:
    /// tools of the level editor (sidebar); Resize/Rotate = transform modes
    enum class Tool {
        None,   /// no tool: plain game view, no selection interactions
        Resize, /// resize mode: hover/click/drag objects in the game view
        Rotate, /// rotate mode: rotate objects around a movable pivot point
        Edit    /// placeholder, not implemented yet
    };

    Editor();

    /** The editor needs access to the world (physics + renderer); called
        after Game was fully constructed. */
    void setWorld(World* world) { mWorld = world; }

    void createEditorWidgets(bool* p_open);

    /// forwards game-view mouse events (world pixel position included);
    /// called from Game::processEvents while the editor is open
    void handleMouseEvent(const sf::Event& event,
                          const sf::Vector2f& mouseWorldPosPixels);

    /// arrow-key navigation through the object list; called from
    /// Game::processEvents while the editor is open
    void handleKey(sf::Keyboard::Scancode scancode);

    bool showLevelEditor = false;

    /// transform mode = the Resize or Rotate tool is turned on (physics is
    /// paused while it is active so dragged bodies don't fall between updates)
    bool transformModeActive() const
    {
        return mActiveTool == Tool::Resize || mActiveTool == Tool::Rotate;
    }

  private:
    enum class DragMode { None, Move, Resize, Rotate, MovePivot };

    void switchLevel(ELevel level);
    void addObject();
    void deleteObject();
    void applyEdits();
    void saveLevel();
    /// edit-buffers from the MAIN CHARACTER physics body (list pseudo-entry)
    void syncCharEditBuffers();
    
    // snapping helpers
    void applySnapping(sf::Vector2f& positionPx, const sf::Vector2f& sizePx) const;
    void setSnapDirectionVertical(bool up);
    void setSnapDirectionHorizontal(bool left);

    /// selection-mode (see class comment)
    int resizeHandleAt(const sf::Vector2f& mouseWorldPosPx) const;
    /// rotate-mode: index of the rotation marker under the cursor, -1 = none
    int rotateMarkerAt(const sf::Vector2f& mouseWorldPosPx) const;
    /// rotate-mode: true if the cursor is on the pivot point marker
    bool pivotMarkerAt(const sf::Vector2f& mouseWorldPosPx) const;
    bool insideSelectedObject(const sf::Vector2f& mouseWorldPosPx) const;
    /// world-space position of the pivot of the selected object
    sf::Vector2f selectedPivotWorldPx() const;
    /// pushes the pivot marker (world px) to the renderer
    void updatePivotMarker();
    void beginDrag(DragMode mode, int handle,
                   const sf::Vector2f& mouseWorldPosPx);
    void updateDrag(const sf::Vector2f& mouseWorldPosPx);
    void endDrag();
    void exitSelectionMode();
    void syncEditBuffersFromSelection();
    void drawSelectionModeHud();
    void drawToolSidebar();
    void drawGridControls();
    void switchTool(Tool tool);
    void drawObjectDetails(SceneObjectPhysicsParameters *object);

    World* mWorld;
    ELevel mSelectedLevel;
    /// index into the selected level's sceneObjects vector, -1 = none
    int mSelectedIndex;
    /// selection-change detection so edit buffers are only re-synced on switch
    int mLastSelectedIndex;

    // edit buffers of the currently selected object
    std::string mEditName;
    bool mEditIsDynamic;
    int mEditTextureId;
    int mEditEntityType;
    int mEditLayer;
    float mEditPositionX;
    float mEditPositionY;
    float mEditWidth;
    float mEditHeight;
    float mEditAngle; /// degrees, editor-facing (radians in the physics world)

    // drag state; the Select tool gates all selection interactions
    DragMode mDragMode;
    Tool mActiveTool;
    int mDragHandle;
    /// object center px at drag start (position + size for the resize math)
    sf::Vector2f mDragStartCenterPx;
    sf::Vector2f mDragStartSizePx;
    /// handle center at drag start (needed for live resize math)
    sf::Vector2f mDragHandleStartPx;
    /// final geometry of the running resize drag (committed on release)
    sf::Vector2f mLastPreviewCenterPx;
    sf::Vector2f mLastPreviewSizePx;
    // rotate-mode state: pivot offset in the unrotated object frame (px,
    // relative to the object center; 0 = center), the world-space pivot
    // captured at drag start and the angles a rotate-drag started with
    sf::Vector2f mPivotOffsetPx;
    sf::Vector2f mRotateStartPivotPx;
    float mRotateStartMouseAngle;
    float mRotateStartObjectAngle;
    b2BodyId mSelectedBodyId;
    b2BodyId mHoveredBodyId;

    // grid-mode overlay (for later tile/tileset creation); pushed to the
    // renderer on every change so the grid updates live
    bool mShowGrid;
    int mGridCellWidth;
    int mGridCellHeight;
    int mGridColorIndex;

    // snapping settings: only one per dimension can be active
    bool mSnapUp;
    bool mSnapDown;
    bool mSnapLeft;
    bool mSnapRight;

    /// the "MainCharacter" pseudo-entry of the object list is selected
    /// (moves the character body; mutually exclusive with the object rows)
    bool mMainCharRowSelected;
};

} // namespace RyuLevel
