/**
 * ImGui level editor (open with F3).
 *
 * Creates and edits the level data: physics boxes (Box2D) are no longer
 * hardcoded in Physics.h but stored as generic per-level JSON files
 * (assets/levels/levelN.json, see LevelData.h) and created/edited live
 * through the incremental Physics API.
 *
 * Selection mode (mouse in the game view, while this editor is open):
 * - hovering an object highlights it (orange transparent + red outline)
 * - clicking an object selects it and shows 8 resize handles
 *   (corners + edge midpoints); dragging a handle resizes the object live
 * - clicking inside the selected object and dragging moves it live;
 *   releasing fixes the position and leaves the selection mode
 * - left-clicking anywhere else in the game view exits the selection mode
 * While the selection mode is active a red "selection-mode active" text
 * is shown at the bottom right of the game view.
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
    /// tools of the level editor (sidebar); Select = selection mode
    enum class Tool {
        None,   /// no tool: plain game view, no selection interactions
        Select, /// selection mode: hover/click/drag objects in the game view
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

  private:
    enum class DragMode { None, Move, Resize };

    void switchLevel(ELevel level);
    void addObject();
    void deleteObject();
    void applyEdits();
    void saveLevel();

    /// selection-mode (see class comment)
    int resizeHandleAt(const sf::Vector2f& mouseWorldPosPx) const;
    bool insideSelectedObject(const sf::Vector2f& mouseWorldPosPx) const;
    void beginDrag(DragMode mode, int handle,
                   const sf::Vector2f& mouseWorldPosPx);
    void updateDrag(const sf::Vector2f& mouseWorldPosPx);
    void endDrag();
    void exitSelectionMode();
    void syncEditBuffersFromSelection();
    void drawSelectionModeHud();
    void drawToolSidebar();
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
    b2BodyId mSelectedBodyId;
    b2BodyId mHoveredBodyId;
};

} // namespace RyuLevel
