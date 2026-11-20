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
#include <SFML/Graphics/Texture.hpp>
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>

#include <imgui.h>
#include <Thirdparty/imgui-filebrowser/imfilebrowser.h>

#include <algorithm>
#include <deque>
#include <functional>
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
        None,        /// no tool: plain game view, no selection interactions
        Resize,      /// resize mode: hover/click/drag objects in the game view
        Rotate,      /// rotate mode: rotate objects around a movable pivot point
        Edit,        /// placeholder, not implemented yet
        TilePlacer,  /// paint grid cells with the selected tileset tile
        TileRemover  /// erase tile-sized objects from grid cells
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

    /// arrow-key navigation through the object list + undo/redo shortcuts
    /// (Ctrl+Z undo, Ctrl+Shift+Z / Ctrl+Y redo); called from
    /// Game::processEvents while the editor is open
    void handleKey(const sf::Event::KeyPressed& key);

    /// undoes the last recorded object edit (Ctrl+Z)
    void undo();
    /// redoes the last undone object edit (Ctrl+Shift+Z / Ctrl+Y)
    void redo();
    /// Tab: cycles the tool None -> Resize -> Rotate -> None
    void cycleTool();

    bool showLevelEditor = false;

    /// zoom factor of the scene region in the enlarged editor window
    /// (1x = native 640x360 pixels; higher values make the scene bigger
    /// for precise edits on large monitors); the game window layout uses
    /// this, so changing it re-arranges view and editor panels
    float sceneZoom() const { return mSceneZoom; }
    /// restores a persisted zoom factor (clamped to >= 1x)
    void setSceneZoom(float zoom) { mSceneZoom = std::max(zoom, 1.f); }

    /// play-in-editor test: while active the game runs with player
    /// control inside the fullscreen editor window; ESC stops it and the
    /// editor stays open (tools must not fight the running game)
    bool testPlayingActive() const { return mTestPlaying; }

    /// reference-image window (level-building aid): a loaded image is
    /// shown in its own resizable window with zoom/fit and scroll-pan;
    /// path, zoom, opacity and window-open state persist in the editor
    /// settings json
    void setReferenceImageVisible(bool visible)
    {
        mShowReferenceImage = visible;
    }
    void setTestPlaying(bool active);

    /// zoom actually used to render: the chosen factor adapted to the
    /// given window so the scene (with its panel margins) always fits;
    /// never modifies the persisted value
    float sceneZoomForWindow(float windowWidthPx, float windowHeightPx) const
    {
        const float maxZoomX = (windowWidthPx - 380.f) / 640.f;
        const float maxZoomY = (windowHeightPx - 260.f) / 360.f;
        return std::min(mSceneZoom,
                        std::max(1.f, std::min(maxZoomX, maxZoomY)));
    }

    /// hook the Game uses to persist the whole editor layout (play
    /// window, zoom, ImGui window positions via imgui.ini); invoked by
    /// the settings window's 'Save as default' button
    void setLayoutSaveHook(std::function<void()> hook)
    {
        mLayoutSaveHook = std::move(hook);
    }

    /// transform mode = the Resize or Rotate tool is turned on (physics is
    /// paused while it is active so dragged bodies don't fall between updates)
    bool tileToolActive() const
    {
        return mActiveTool == Tool::TilePlacer || mActiveTool == Tool::TileRemover;
    }
    bool transformToolActive() const
    {
        return mActiveTool == Tool::Resize || mActiveTool == Tool::Rotate;
    }
    /// true while any editor tool interacts with the game view (physics is
    /// paused during that so painted/dragged bodies don't move between updates)
    bool transformModeActive() const
    {
        return mActiveTool == Tool::Resize || mActiveTool == Tool::Rotate
            || tileToolActive();
    }

  private:
    enum class DragMode { None, Move, Resize, Rotate, MovePivot };

    /// scene zoom chosen in the tool sidebar (see sceneZoom()); the editor
    /// window layout scales the native canvas region by this factor
    float mSceneZoom{2.f};

    /// true while the level is being played back inside the editor
    /// (see setTestPlaying)
    bool mTestPlaying{false};

    /// editor-was-open tracking: the createEditorWidgets close-branch
    /// (visual cleanup) must run once when the editor closes, NOT every
    /// frame while it stays closed (it would reset e.g. the physics
    /// debug overlay in play mode)
    bool mEditorWasOpen{false};

    /// grid state before the play test: the grid overlay is hidden while
    /// the level is played (editing aid) and restored afterwards
    bool mGridShownBeforeTest{false};

    /// reference-image preview: texture loaded from the chosen file plus
    /// its display settings (persisted via the editor-settings json)
    bool mShowReferenceImage{false};
    sf::Texture mReferenceTexture;
    bool mReferenceLoaded{false};
    std::string mReferencePath;
    float mReferenceZoom{1.f};
    float mReferenceOpacity{1.f};
    std::unique_ptr<ImGui::FileBrowser> mReferenceFileBrowser;

    /// loads the reference image from a file path (logs on failure)
    void loadReferenceImage(const std::string& path);
    /// draws the reference-image window (called while the editor is open)
    void drawReferenceImageWindow();

    /// optional persistence hook (see setLayoutSaveHook)
    std::function<void()> mLayoutSaveHook;

    /// zoom the editor panels dock against: the effective (window-adapted)
    /// scene zoom, so panels dock to the scene edge that is really shown
    float dockZoom() const;

    /// undo/redo record: state of one scene object before/after an edit,
    /// addressed by level + index into the level's sceneObjects vector;
    /// kind == Delete/Add entries use the states to re-insert or remove
    /// the object itself (STRG+Z also restores deleted objects)
    struct UndoEntry
    {
        enum class Kind { Edit, Delete, Add, StrokeAdd, StrokeDelete };
        ELevel level;
        int index;
        Kind kind;
        SceneObjectPhysicsParameters before;
        SceneObjectPhysicsParameters after;
        /// tile strokes: one entry per drag, n objects;
        /// StrokeAdd: indices ascending = stroke base + k (tail objects);
        /// StrokeDelete: indices/params in ORIGINAL removal order (descending
        /// indices, so each index is valid in the list state of its time)
        std::vector<int> strokeIndices;
        std::vector<SceneObjectPhysicsParameters> strokeParams;
        /// replace-mode strokes also remove objects (recorded in their
        /// original removal order, like StrokeDelete)
        std::vector<int> strokeRemovedIndices;
        std::vector<SceneObjectPhysicsParameters> strokeRemovedParams;
    };

    /// one object removed during a tile-erase stroke (its removal-time index
    /// + its complete state, needed to re-insert it on undo)
    struct StrokeRemoved
    {
        int index;
        SceneObjectPhysicsParameters params;
    };

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
    /// undo/redo internals
    void pushUndoEntry(const UndoEntry& entry);
    /// restores an entry's before/after state and recreates the body;
    /// returns false when the entry no longer matches the level data
    bool applyUndoState(const UndoEntry& entry, bool redoState);
    /// removes the object at \p index (structural undo step); keeps the
    /// remaining undo entries aligned via adjustUndoIndicesForRemoval
    bool removeSceneObjectAtIndex(ELevel level, int index);
    /// re-inserts \p params at \p index (undo of a delete / redo of an add)
    bool reinsertSceneObject(ELevel level, int index,
                             const SceneObjectPhysicsParameters& params);
    /// object at \p index is gone: older entries shift up, entries of the
    /// removed object die with it (its pre-delete state is kept in the
    /// Delete entry itself)
    void adjustUndoIndicesForRemoval(ELevel level, int index);
    /// an object was re-inserted at \p index: entries at/above it shift down
    void adjustUndoIndicesForInsertion(ELevel level, int index);
    /// true when the two states differ in any editable field
    bool objectStatesDiffer(const SceneObjectPhysicsParameters& lhs,
                            const SceneObjectPhysicsParameters& rhs) const;
    /// opens the rename window for the object list row \p index
    void beginRename(int index);
    /// small modal window: rename the object (double-click in the list)
    void drawRenameWindow();
    /// create-level window: name input, creates an empty level (floor only,
    /// character centered) in the next free level slot
    void drawCreateLevelWindow();
    /// creates level \p name in the next free slot; returns false when no
    /// slot is free or the name is empty
    bool createLevelFromName(const std::string& name);
    /// editor-settings window (undo steps, settings file)
    void drawEditorSettings();
    void saveEditorSettings();
    void loadEditorSettings();
    void drawSelectionModeHud();
    void drawToolSidebar();
    void drawGridControls();
    /// per-layer parallax sliders + camera preview pan (level data is
    /// persisted in the level JSON root "parallax" key on save)
    void drawParallaxControls();
    void switchTool(Tool tool);
    /// plain click in the game view without a transform tool: selects the
    /// object under the cursor (list row + edit buffers + fill highlight)
    void selectObjectAt(const sf::Vector2f& mouseWorldPosPx);
    /// right click in the game view opens the tools menu at the cursor
    void openToolContextMenu(const sf::Vector2i& screenPosPx);
    /// the tools context window (resize/rotate/undo/redo)
    void drawToolContextMenu();
    void drawObjectDetails(SceneObjectPhysicsParameters *object);

    // tile placement tools (TilePlacer/TileRemover): the active tile brush is
    // the current texture edit buffer (tileset id + tile coordinates)

    /// world-pixel cell index under the cursor (grid cell coordinates)
    sf::Vector2i tileCellAt(const sf::Vector2f& posPx) const;
    /// world-pixel center of a grid cell
    sf::Vector2f tileCellCenterPx(const sf::Vector2i& cell) const;
    /// true when any scene object of the active level overlaps \p centerPx
    bool tileCellOccupied(const sf::Vector2f& centerPx) const;
    /// syncs the grid overlay cell size to the current brush tile + shows it
    void syncTileGridToBrush();
    /// begins a paint/erase stroke (left/right button resp.)
    void beginTileStroke(bool erase, const sf::Vector2f& posPx);
    /// paints/erases every grid cell on the line posPx..prevPx (drag)
    void updateTileStroke(const sf::Vector2f& posPx);
    /// commits the stroke as ONE undo entry (paint = n appended objects,
    /// erase = n removed objects; n == 0 pushes nothing)
    void endTileStroke();
    /// places one tile object into the cell under \p posPx
    void placeTileAt(const sf::Vector2f& cellCenterPx);
    /// erases the tile-sized objects of the cell under \p posPx
    void eraseTileAt(const sf::Vector2f& cellCenterPx);
    /// texture picker window: registry thumbnails (whole images + tilesets)
    /// with a magnified hover preview; picking applies the edit directly
    void drawTexturePicker();
    /// thumbnail of the current texture edit buffer (picker button face)
    void drawTexturePreview(const sf::Vector2f& size);

    World* mWorld;
    ELevel mSelectedLevel;
    /// index into the selected level's sceneObjects vector, -1 = none
    int mSelectedIndex;
    /// selection-change detection so edit buffers are only re-synced on switch
    int mLastSelectedIndex;

    // edit buffers of the currently selected object
    std::string mEditName;
    bool mEditIsDynamic;
    bool mEditHasPhysics;
    /// scene-texture registry id of the object (empty = none)
    std::string mEditTextureId;
    /// tile of a tileset texture (-1/-1 = whole image)
    int mEditTileCol;
    int mEditTileRow;
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
    /// object rotation (rad) at drag start; resize math runs in the local
    /// (unrotated) object frame and re-rotates the result with it
    float mDragStartRotation;
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

    // undo/redo: at most mMaxUndoSteps recorded edits; every new edit
    // invalidates the redo stack
    std::deque<UndoEntry> mUndoStack;
    std::deque<UndoEntry> mRedoStack;
    int mMaxUndoSteps;
    /// object state captured at drag start (pushed on endDrag into the
    /// undo stack when the drag actually changed the object)
    SceneObjectPhysicsParameters mDragUndoSnapshot;
    int mDragUndoIndex;
    bool mDragUndoValid;

    // rename window (double-click on an object list row)
    char mRenameBuffer[64];
    int mRenameIndex;
    bool mRenameActive;

    // texture picker window (open from the object details)
    bool mShowTexturePicker;

    // create-level window (level combo's '+ create' button)
    bool mShowCreateLevel;
    char mNewLevelName[64];

    // tile stroke state (TilePlacer left=paint/right=erase, TileRemover erase)
    bool mTileStrokeActive;
    bool mTileStrokeErase;
    sf::Vector2f mTileStrokePrevPosPx;
    bool mTileStrokePrevValid;
    std::vector<int> mTileStrokeIndices;
    std::vector<SceneObjectPhysicsParameters> mTileStrokePlacedParams;
    std::vector<StrokeRemoved> mTileStrokeRemoved;
    /// brush-cell preview in the game view (orange overlay, updateTileStroke)
    bool mShowTileBrushPreview;
    /// place strokes replace tile-sized objects in occupied cells
    /// (big manually built geometry is never replaced)
    bool mReplaceTiles;

    // parallax factors of the selected level (Background, Ground1,
    // Foreground) + camera preview offset (parallax pan preview)
    float mParallax[3];
    sf::Vector2f mCameraPreviewOffsetPx;
    /// parallax buffers must re-sync from the renderer when the editor
    /// (re-)opens (the level may have been changed/saved in between)
    bool mParallaxSyncNeeded;

    // editor settings window + persistent settings file
    bool mShowEditorSettings;
    char mSettingsPath[128];

    // tools context menu (right click in the game view): screen-space
    // position + window rect (to keep clicks off the menu from closing it)
    bool mShowToolContextMenu;
    int mContextMenuPosX;
    int mContextMenuPosY;
    float mContextMenuLeft;
    float mContextMenuTop;
    float mContextMenuWidth;
    float mContextMenuHeight;
};

} // namespace RyuLevel
