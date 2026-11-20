#include <RyuSuite/LevelEditor.h>

#include <Ryu/Core/World.h>
#include <Ryu/Physics/Physics.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Scene/LevelData.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Core/Renderer.h>

#include <imgui.h>
#include <imgui-SFML.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <optional>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

namespace RyuLevel
{

namespace
{

constexpr int kNumBodyTypes = 2;
const char *kBodyTypeNames[kNumBodyTypes] = { "Static", "Dynamic" };

constexpr int kNumEntityTypes = 2;
const char *kEntityTypeNames[kNumEntityTypes] = { "None", "Climbable" };

/// resize handles: 4 corners + 4 edge midpoints; masks apply per axis
struct ResizeHandle
{
    sf::Vector2f relativePos; /// -1/0/1 per axis, relative to center/half-size
    int maskX; /// resize direction on the x axis
    int maskY;
};

constexpr int kNumHandles = 8;
constexpr float kHandleSize = 4.f;      /// px, visual size of one handle
constexpr float kHandleHitRadius = 6.f; /// px, click tolerance
constexpr float kMinObjectSize = 4.f;   /// px

constexpr ResizeHandle kHandles[kNumHandles] = {
    {{-1.f, -1.f}, -1, -1}, {{0.f, -1.f}, 0, -1}, {{1.f, -1.f}, 1, -1},
    {{-1.f, 0.f}, -1, 0}, {{1.f, 0.f}, 1, 0},
    {{-1.f, 1.f}, -1, 1}, {{0.f, 1.f}, 0, 1}, {{1.f, 1.f}, 1, 1},
};

/// minimum side length so corner + midpoint markers fit next to each
/// other (must match the renderer's kMarkerFootprint)
constexpr float kMarkerFootprint = 12.f;
/// rotate-mode: 4 corner markers (arcs around the corners), hit tolerance
constexpr float kRotateMarkerHitRadius = 8.f;
/// rotate-mode: hit tolerance of the pivot-point marker
constexpr float kPivotHitRadius = 7.f;

/// rotates a local offset (unrotated object frame) by \p angleRad (world px)
sf::Vector2f
rotateOffset(const sf::Vector2f& localPx, float angleRad)
{
    const float c = std::cos(angleRad);
    const float s = std::sin(angleRad);
    return { localPx.x * c - localPx.y * s, localPx.x * s + localPx.y * c };
}

/// angle (radians) of a 2d vector, used for the rotate-drag math
float
vectorAngle(const sf::Vector2f& v)
{
    return std::atan2(v.y, v.x);
}

/// mouseover tooltip of the last ImGui item (call right after the widget):
/// texture id + rendered size in pixels (tile size + full image size
/// when \p col/\p row address a tile of a tileset)
void
textureSizeTooltip(const SceneTextureRegistry& registry,
                   const std::string& id, int col, int row)
{
    if (not ImGui::IsItemHovered()) { return; }
    const sf::Texture* texture = registry.texture(id);
    if (texture == nullptr)
    {
        ImGui::SetTooltip("%s\n(not found)", id.c_str());
        return;
    }
    const sf::Vector2u image = texture->getSize();
    const sf::IntRect rect = registry.textureRect(id, col, row);
    const auto* info = registry.tileset(id);
    if (col >= 0 and info != nullptr)
    {
        ImGui::SetTooltip(
            "%s [%d,%d]\ntile: %dx%d px (grid %dx%d)\nimage: %ux%u px",
            id.c_str(), col, row, rect.size.x, rect.size.y,
            info->columns, info->rows, image.x, image.y);
    }
    else
    {
        ImGui::SetTooltip("%s\n%dx%d px",
                         id.c_str(), rect.size.x, rect.size.y);
    }
}

} // anonymous namespace

namespace
{

bool
bodyIdsEqual(const b2BodyId& lhs, const b2BodyId& rhs)
{
    return lhs.index1 == rhs.index1 && lhs.generation == rhs.generation
           && lhs.world0 == rhs.world0;
}

} // anonymous namespace (body ids)

Editor::Editor()
    : mWorld(nullptr)
    , mSelectedLevel(ELevel::Level2)
    , mSelectedIndex(-1)
    , mLastSelectedIndex(-1)
    , mEditName("new_object")
    , mEditIsDynamic(false)
    , mEditHasPhysics(true)
    , mEditTextureId("Grass")
    , mEditTileCol(-1)
    , mEditTileRow(-1)
    , mEditEntityType(0)
    , mEditLayer(1)
    , mEditPositionX(320.f)
    , mEditPositionY(180.f)
    , mEditWidth(64.f)
    , mEditHeight(16.f)
    , mEditAngle(0.f)
    , mDragMode(DragMode::None)
    , mDragHandle(-1)
    , mDragStartCenterPx({})
    , mDragStartSizePx({})
    , mDragStartRotation(0.f)
    , mDragHandleStartPx({})
    , mLastPreviewCenterPx({})
    , mLastPreviewSizePx({})
    , mPivotOffsetPx({})
    , mRotateStartPivotPx({})
    , mRotateStartMouseAngle(0.f)
    , mRotateStartObjectAngle(0.f)
    , mActiveTool(Tool::None)
    , mSelectedBodyId(b2_nullBodyId)
    , mHoveredBodyId(b2_nullBodyId)
    , mShowGrid(false)
    , mGridCellWidth(32)
    , mGridCellHeight(32)
    , mGridColorIndex(0)
    , mMainCharRowSelected(false)
    , mSnapUp(false)
    , mSnapDown(false)
    , mSnapLeft(false)
    , mSnapRight(false)
    , mMaxUndoSteps(30)
    , mDragUndoSnapshot()
    , mDragUndoIndex(-1)
    , mDragUndoValid(false)
    , mRenameBuffer{}
    , mRenameIndex(-1)
    , mRenameActive(false)
    , mShowTexturePicker(false)
    , mShowCreateLevel(false)
    , mNewLevelName{}
    , mShowEditorSettings(false)
    , mShowToolContextMenu(false)
    , mContextMenuPosX(0)
    , mContextMenuPosY(0)
    , mContextMenuLeft(0.f)
    , mContextMenuTop(0.f)
    , mContextMenuWidth(0.f)
    , mContextMenuHeight(0.f)
    , mTileStrokeActive(false)
    , mTileStrokeErase(false)
    , mTileStrokePrevPosPx()
    , mTileStrokePrevValid(false)
    , mTileStrokeIndices()
    , mTileStrokePlacedParams()
    , mTileStrokeRemoved()
    , mShowTileBrushPreview(false)
    , mReplaceTiles(false)
    , mParallax{ 0.5f, 1.f, 1.2f }
    , mCameraPreviewOffsetPx()
    , mParallaxSyncNeeded(true)
{
    std::snprintf(mSettingsPath, sizeof(mSettingsPath),
                  "%s", "assets/levelEditorSettings.json");
    loadEditorSettings();
}

void
Editor::switchLevel(ELevel level)
{
    exitSelectionMode();
    // the level data is reloaded: undo entries (level + object index) no
    // longer match the rebuilding scene-object vectors
    mUndoStack.clear();
    mRedoStack.clear();
    mSelectedLevel = level;
    mSelectedIndex = -1;
    mMainCharRowSelected = false;
    if (mWorld)
    {
        mWorld->setCharacterSelection({}, {}, false);
        mWorld->reloadSceneObjects(level);
        syncEditBuffersFromSelection();
        for (std::size_t i = 0; i < 3; ++i)
        {
            mParallax[i] = mWorld->getLayerParallax(i);
        }
        fmt::print("LevelEditor: switched to level {}.\n", level._to_string());
    }
}

void
Editor::addObject()
{
    if (not mWorld) { return; }
    SceneObjectPhysicsParameters object{
        { mEditPositionX, mEditPositionY },
        { mEditWidth, mEditHeight },
        mEditName,
        mEditIsDynamic ? b2_dynamicBody : b2_staticBody,
        mEditTextureId,
        mEditEntityType == 1 ? EntityType::Climbable : EntityType::None,
        mEditLayer,
        0.f, // rotation: radians in the physics world
        mEditHasPhysics,
        mEditTileCol,
        mEditTileRow
    };
    auto& objectsBefore = mWorld->getSceneObjects().at(mSelectedLevel);
    auto& added = mWorld->addSceneObject(mSelectedLevel, std::move(object));
    mSelectedIndex = static_cast<int>(objectsBefore.size()) - 1;
    mLastSelectedIndex = -1; // refresh edit buffers from the new object
    // record the add so STRG+Z removes the object again
    pushUndoEntry({ mSelectedLevel, mSelectedIndex, UndoEntry::Kind::Add,
                    {}, added });
    syncEditBuffersFromSelection();
    fmt::print("LevelEditor: added object '{}'.\n", added.mName);
}

void
Editor::deleteObject()
{
    if (not mWorld || mSelectedIndex < 0) { return; }
    auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
    if (mSelectedIndex >= static_cast<int>(objects.size())) { return; }
    // record the delete so STRG+Z restores the object at its old position;
    // the pre-removal state (geometry, name, ...) is kept in the entry
    UndoEntry entry{ mSelectedLevel, mSelectedIndex, UndoEntry::Kind::Delete,
                     objects.at(mSelectedIndex), {} };
    exitSelectionMode();
    mWorld->removeSceneObject(objects.at(entry.index).mPhysicsBodyId);
    // rows after the removed one shift up: keep the remaining undo entries
    // pointing at their objects; older entries on the removed slot die
    // with the object (the Delete entry restores its pre-delete state)
    adjustUndoIndicesForRemoval(entry.level, entry.index);
    pushUndoEntry(entry);
    if (mSelectedIndex >= static_cast<int>(objects.size()))
    {
        mSelectedIndex = static_cast<int>(objects.size()) - 1;
    }
}

void
Editor::applyEdits()
{
    if (not mWorld) { return; }
    if (mMainCharRowSelected)
    {
        // the character body shape is animation-driven: position-only edits
        const sf::Vector2f position{ mEditPositionX, mEditPositionY };
        mWorld->moveMainCharacterToPx(position);
        mWorld->setCharacterSelection(position, { mEditWidth, mEditHeight }, true);
        syncCharEditBuffers();
        return;
    }
    if (mSelectedIndex < 0) { return; }
    auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
    if (mSelectedIndex >= static_cast<int>(objects.size())) { return; }

    auto& object = objects.at(mSelectedIndex);
    const UndoEntry undoEntry{ mSelectedLevel, mSelectedIndex,
                               UndoEntry::Kind::Edit, object, object };
    object.mName = mEditName;
    object.mType = mEditIsDynamic ? b2_dynamicBody : b2_staticBody;
    object.mHasPhysics = mEditHasPhysics;
    object.mTextureId = mEditTextureId;
    object.mTileCol = mEditTileCol;
    object.mTileRow = mEditTileRow;
    object.mEntityType = mEditEntityType == 1 ? EntityType::Climbable
                                              : EntityType::None;
    object.mLayer = mEditLayer;
    object.mRotation = Converter::degToRad(mEditAngle);
    object.mPosition = { Converter::pixelsToMeters(mEditPositionX),
                         Converter::pixelsToMeters(mEditPositionY) };
    object.mSize = { Converter::pixelsToMeters(mEditWidth),
                     Converter::pixelsToMeters(mEditHeight) };

    // recreate and keep the in-level selection highlight on the new body
    mSelectedBodyId = mWorld->updateSceneObject(object.mPhysicsBodyId);
    mWorld->setSelectedHighlight(mSelectedBodyId);

    UndoEntry entry = undoEntry;
    entry.after = object;
    if (objectStatesDiffer(entry.before, entry.after))
    {
        pushUndoEntry(entry);
    }
}

void
Editor::saveLevel()
{
    if (not mWorld) { return; }
    const RyuLevel::ParallaxFactors parallax{ mParallax[0], mParallax[1],
                                                mParallax[2] };
    RyuLevel::saveLevelData(mSelectedLevel,
                            mWorld->getSceneObjects().at(mSelectedLevel),
                            "assets/levels", &parallax);
}

/// ---- selection mode ----------------------------------------------------

int
Editor::resizeHandleAt(const sf::Vector2f &mouseWorldPosPx) const
{
    SceneObjectPhysicsParameters* object
        = mWorld ? mWorld->findSceneObject(mSelectedBodyId) : nullptr;
    if (object == nullptr || mDragMode != DragMode::None) { return -1; }

    const sf::Vector2f centerPx{
        Converter::metersToPixels(object->mPosition.x),
        Converter::metersToPixels(object->mPosition.y)};
    const sf::Vector2f halfPx{
        Converter::metersToPixels(object->mSize.x) / 2.f,
        Converter::metersToPixels(object->mSize.y) / 2.f};
    const float angleRad = mWorld->getSceneObjectRotation(mSelectedBodyId);

    // edge-midpoint handles are dropped on small objects (same rule as the
    // renderer draws them), so hit-testing matches the visible markers
    const bool hasHorizontalMid = 2.f * halfPx.x >= 3.f * kMarkerFootprint;
    const bool hasVerticalMid = 2.f * halfPx.y >= 3.f * kMarkerFootprint;

    int nearestHandle = -1;
    float bestDistanceSq = kHandleHitRadius * kHandleHitRadius;
    for (int i = 0; i < kNumHandles; ++i)
    {
        if (kHandles[i].relativePos.x == 0.f && not hasHorizontalMid) { continue; }
        if (kHandles[i].relativePos.y == 0.f && not hasVerticalMid) { continue; }
        const sf::Vector2f handleCenterPx = centerPx
            + rotateOffset({ halfPx.x * kHandles[i].relativePos.x,
                             halfPx.y * kHandles[i].relativePos.y }, angleRad);
        const sf::Vector2f distance = mouseWorldPosPx - handleCenterPx;
        const float distanceSq = distance.x * distance.x + distance.y * distance.y;
        if (distanceSq <= bestDistanceSq)
        {
            // pick the NEAREST handle: corners and edge midpoints sit close
            // together on thin objects, first-match would prefer corners and
            // activate a second resize axis
            nearestHandle = i;
            bestDistanceSq = distanceSq;
        }
    }
    return nearestHandle;
}

int
Editor::rotateMarkerAt(const sf::Vector2f &mouseWorldPosPx) const
{
    SceneObjectPhysicsParameters* object
        = mWorld ? mWorld->findSceneObject(mSelectedBodyId) : nullptr;
    if (object == nullptr || mDragMode != DragMode::None) { return -1; }

    const sf::Vector2f centerPx{
        Converter::metersToPixels(object->mPosition.x),
        Converter::metersToPixels(object->mPosition.y)};
    const sf::Vector2f halfPx{
        Converter::metersToPixels(object->mSize.x) / 2.f,
        Converter::metersToPixels(object->mSize.y) / 2.f};
    const float angleRad = mWorld->getSceneObjectRotation(mSelectedBodyId);

    int nearestMarker = -1;
    float bestDistanceSq = kRotateMarkerHitRadius * kRotateMarkerHitRadius;
    for (int i = 0; i < 4; ++i)
    {
        const float signX = (i % 2 == 0) ? -1.f : 1.f;
        const float signY = (i < 2) ? -1.f : 1.f;
        const sf::Vector2f markerPx = centerPx
            + rotateOffset({ signX * halfPx.x, signY * halfPx.y }, angleRad);
        const sf::Vector2f distance = mouseWorldPosPx - markerPx;
        const float distanceSq = distance.x * distance.x + distance.y * distance.y;
        if (distanceSq <= bestDistanceSq)
        {
            nearestMarker = i;
            bestDistanceSq = distanceSq;
        }
    }
    return nearestMarker;
}

bool
Editor::pivotMarkerAt(const sf::Vector2f &mouseWorldPosPx) const
{
    if (mWorld == nullptr || B2_IS_NULL(mSelectedBodyId)) { return false; }
    const sf::Vector2f distance = mouseWorldPosPx - selectedPivotWorldPx();
    return distance.x * distance.x + distance.y * distance.y
           <= kPivotHitRadius * kPivotHitRadius;
}

sf::Vector2f
Editor::selectedPivotWorldPx() const
{
    SceneObjectPhysicsParameters* object
        = mWorld ? mWorld->findSceneObject(mSelectedBodyId) : nullptr;
    if (object == nullptr) { return {}; }
    const sf::Vector2f centerPx{
        Converter::metersToPixels(object->mPosition.x),
        Converter::metersToPixels(object->mPosition.y)};
    const float angleRad = mWorld->getSceneObjectRotation(mSelectedBodyId);
    return centerPx + rotateOffset(mPivotOffsetPx, angleRad);
}

void
Editor::updatePivotMarker()
{
    if (mWorld == nullptr) { return; }
    if (mActiveTool == Tool::Rotate && not B2_IS_NULL(mSelectedBodyId)
        && mWorld->findSceneObject(mSelectedBodyId) != nullptr)
    {
        mWorld->setPivotMarker(selectedPivotWorldPx(), true);
    }
    else
    {
        mWorld->setPivotMarker({}, false);
    }
}

bool
Editor::insideSelectedObject(const sf::Vector2f &mouseWorldPosPx) const
{
    SceneObjectPhysicsParameters* object
        = mWorld ? mWorld->findSceneObject(mSelectedBodyId) : nullptr;
    if (object == nullptr) { return false; }

    const sf::Vector2f centerPx{
        Converter::metersToPixels(object->mPosition.x),
        Converter::metersToPixels(object->mPosition.y)};
    const sf::Vector2f halfPx{
        Converter::metersToPixels(object->mSize.x) / 2.f,
        Converter::metersToPixels(object->mSize.y) / 2.f};
    const float angleRad = mWorld->getSceneObjectRotation(mSelectedBodyId);
    // hit test in the unrotated object frame (rotation-aware)
    const sf::Vector2f localPx
        = rotateOffset(mouseWorldPosPx - centerPx, -angleRad);
    return std::abs(localPx.x) <= halfPx.x && std::abs(localPx.y) <= halfPx.y;
}

void
Editor::beginDrag(DragMode mode, int handle, const sf::Vector2f &mouseWorldPosPx)
{
    SceneObjectPhysicsParameters* object = nullptr;
    if (mMainCharRowSelected)
    {
        // the character body: geometry from the live physics rect, no
        // physics-object recreation involved in its drags
        const Physics::CharacterPhysicsRect rect = mWorld->getCharacterRect();
        mDragStartSizePx = rect.sizePx;
        mDragStartCenterPx = { Converter::metersToPixels(rect.centerM.x),
                               Converter::metersToPixels(rect.centerM.y) };
    }
    else
    {
        object = mWorld->findSceneObject(mSelectedBodyId);
        if (object == nullptr) { return; }
        mDragStartCenterPx = { Converter::metersToPixels(object->mPosition.x),
                               Converter::metersToPixels(object->mPosition.y) };
        mDragStartSizePx = { Converter::metersToPixels(object->mSize.x),
                             Converter::metersToPixels(object->mSize.y) };
    }

    // undo record: capture the object state at drag start (indices of the
    // level's object vector stay stable during a drag); committed to the
    // undo stack in endDrag when the drag actually changed the object
    mDragUndoIndex = -1;
    mDragUndoValid = false;
    if (object != nullptr)
    {
        mDragUndoSnapshot = *object;
        auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
        for (int i = 0; i < static_cast<int>(objects.size()); ++i)
        {
            if (&objects.at(i) == object) { mDragUndoIndex = i; break; }
        }
        mDragUndoValid = mDragUndoIndex >= 0;
    }

    mDragMode = mode;
    mDragHandle = handle;
    mDragStartRotation = mWorld->getSceneObjectRotation(mSelectedBodyId);
    if (mode == DragMode::Resize)
    {
        // handle position in the rotated world (the drag math itself runs in
        // the unrotated object frame, see updateDrag)
        mDragHandleStartPx = mDragStartCenterPx
            + rotateOffset({ kHandles[handle].relativePos.x
                                * mDragStartSizePx.x / 2.f,
                             kHandles[handle].relativePos.y
                                * mDragStartSizePx.y / 2.f },
                           mDragStartRotation);
        // commit fallback for a press+release without any mouse movement
        mLastPreviewCenterPx = mDragStartCenterPx;
        mLastPreviewSizePx = mDragStartSizePx;
    }
    else if (mode == DragMode::Rotate)
    {
        // rotation happens around the world-space pivot captured here; the
        // mouse angle relative to it drives the live rotation delta
        mRotateStartPivotPx = selectedPivotWorldPx();
        mRotateStartMouseAngle
            = vectorAngle(mouseWorldPosPx - mRotateStartPivotPx);
        mRotateStartObjectAngle
            = mWorld->getSceneObjectRotation(mSelectedBodyId);
        updatePivotMarker();
    }
    else if (mode == DragMode::MovePivot)
    {
        // pivot drags follow the cursor directly (clamped to the object
        // borders in updateDrag); no additional start state needed
    }
    else
    {
        // keep the grab offset between cursor and object center; otherwise
        // moving snaps the center under the cursor
        mDragHandleStartPx = mouseWorldPosPx;
    }
}

void
Editor::updateDrag(const sf::Vector2f &mouseWorldPosPx)
{
    if (mMainCharRowSelected && mDragMode == DragMode::Move)
    {
        // live character movement (transform only, no body recreation)
        const sf::Vector2f grabOffset = mDragHandleStartPx - mDragStartCenterPx;
        sf::Vector2f newCenter = mouseWorldPosPx - grabOffset;
        
        // Apply snapping if any snap direction is active
        if (mSnapUp || mSnapDown || mSnapLeft || mSnapRight)
        {
            applySnapping(newCenter, mDragStartSizePx);
        }
        
        mWorld->moveMainCharacterToPx(newCenter);
        mWorld->setCharacterSelection(newCenter, mDragStartSizePx, true);
        mEditPositionX = newCenter.x;
        mEditPositionY = newCenter.y;
        return;
    }

    SceneObjectPhysicsParameters* object = mWorld->findSceneObject(mSelectedBodyId);
    if (object == nullptr) { return; }

    if (mDragMode == DragMode::Move)
    {
        const sf::Vector2f grabOffset = mDragHandleStartPx - mDragStartCenterPx;
        sf::Vector2f newCenter = mouseWorldPosPx - grabOffset;
        
        // Apply snapping if any snap direction is active
        if (mSnapUp || mSnapDown || mSnapLeft || mSnapRight)
        {
            applySnapping(newCenter, mDragStartSizePx);
        }
        
        b2Vec2 position{ Converter::pixelsToMeters<float>(newCenter.x),
                         Converter::pixelsToMeters<float>(newCenter.y) };
        // live movement without body recreation
        mWorld->moveSceneObject(mSelectedBodyId, position);
        // keep the edit fields in sync with the live drag
        mEditPositionX = newCenter.x;
        mEditPositionY = newCenter.y;
    }
    else if (mDragMode == DragMode::Resize)
    {
        const int maskX = kHandles[mDragHandle].maskX;
        const int maskY = kHandles[mDragHandle].maskY;
        const sf::Vector2f halfStart = mDragStartSizePx / 2.f;

        // resize math in the object's LOCAL (unrotated) frame so rotated
        // objects grow along their own edges: the cursor is rotated into
        // local space, the dragged edge follows it there and the opposite
        // (anchored) edge stays put in the world
        const sf::Vector2f localMouse = rotateOffset(
            mouseWorldPosPx - mDragStartCenterPx, -mDragStartRotation);

        // per-axis helper: mask=+1 drags the right/bottom edge, -1 the
        // left/top one, 0 keeps the axis; minimum size enforced
        auto applyAxis = [](int mask, float mouseLocalAxis, float startHalfAxis,
                            float &sizeAxis, float &centerLocalAxis)
        {
            if (mask == 0)
            {
                sizeAxis = 2.f * startHalfAxis;
                centerLocalAxis = 0.f;
                return;
            }
            const float anchoredEdge = -static_cast<float>(mask) * startHalfAxis;
            const float newSize = std::max(
                static_cast<float>(mask) * (mouseLocalAxis - anchoredEdge),
                kMinObjectSize);
            sizeAxis = newSize;
            // center offset (local) from the drag-start center
            centerLocalAxis = static_cast<float>(mask)
                * (newSize / 2.f - startHalfAxis);
        };

        sf::Vector2f newSize = mDragStartSizePx;
        sf::Vector2f centerLocal{};
        applyAxis(maskX, localMouse.x, halfStart.x, newSize.x, centerLocal.x);
        applyAxis(maskY, localMouse.y, halfStart.y, newSize.y, centerLocal.y);
        const sf::Vector2f newCenter
            = mDragStartCenterPx + rotateOffset(centerLocal, mDragStartRotation);

        mLastPreviewCenterPx = newCenter;
        mLastPreviewSizePx = newSize;
        // keep the edit fields in sync with the live drag
        mEditPositionX = newCenter.x;
        mEditPositionY = newCenter.y;
        mEditWidth = newSize.x;
        mEditHeight = newSize.y;
        // live preview overlay in the game view (drawn at the object's
        // rotation); the body is recreated with the final size when the
        // drag ends
        mWorld->setResizePreview(newCenter, newSize, true, mDragStartRotation);
    }
    else if (mDragMode == DragMode::Rotate)
    {
        // rotate the object live around the pivot captured at drag start;
        // the body center travels on an arc, the sprite follows via the
        // per-frame physics-updated events
        const float mouseAngle
            = vectorAngle(mouseWorldPosPx - mRotateStartPivotPx);
        const float newAngle = mRotateStartObjectAngle
            + (mouseAngle - mRotateStartMouseAngle);
        const b2Vec2 pivotM{
            Converter::pixelsToMeters(mRotateStartPivotPx.x),
            Converter::pixelsToMeters(mRotateStartPivotPx.y)};
        mWorld->rotateSceneObject(mSelectedBodyId, newAngle, pivotM);

        // keep the edit fields in sync with the live drag (angle in degrees)
        mEditAngle = static_cast<float>(Converter::radToDeg(newAngle));
        SceneObjectPhysicsParameters* object
            = mWorld->findSceneObject(mSelectedBodyId);
        if (object != nullptr)
        {
            mEditPositionX = Converter::metersToPixels(object->mPosition.x);
            mEditPositionY = Converter::metersToPixels(object->mPosition.y);
        }
    }
    else if (mDragMode == DragMode::MovePivot)
    {
        // move the pivot inside the object borders: the cursor position is
        // transformed into the unrotated object frame and clamped
        SceneObjectPhysicsParameters* object
            = mWorld->findSceneObject(mSelectedBodyId);
        if (object == nullptr) { return; }
        const sf::Vector2f centerPx{
            Converter::metersToPixels(object->mPosition.x),
            Converter::metersToPixels(object->mPosition.y)};
        const sf::Vector2f halfPx{
            Converter::metersToPixels(object->mSize.x) / 2.f,
            Converter::metersToPixels(object->mSize.y) / 2.f};
        const float angleRad
            = mWorld->getSceneObjectRotation(mSelectedBodyId);
        const sf::Vector2f localPx
            = rotateOffset(mouseWorldPosPx - centerPx, -angleRad);
        mPivotOffsetPx = { std::clamp(localPx.x, -halfPx.x, halfPx.x),
                           std::clamp(localPx.y, -halfPx.y, halfPx.y) };
        updatePivotMarker();
    }
}

void
Editor::endDrag()
{
    if (not mWorld) { return; }

    if (mDragMode == DragMode::Resize)
    {
        // commit the previewed geometry: write it into the object and
        // recreate the body once with the final size
        SceneObjectPhysicsParameters* object
            = mWorld->findSceneObject(mSelectedBodyId);
        if (object != nullptr)
        {
            object->mPosition = { Converter::pixelsToMeters(mLastPreviewCenterPx.x),
                                  Converter::pixelsToMeters(mLastPreviewCenterPx.y) };
            object->mSize = { Converter::pixelsToMeters(mLastPreviewSizePx.x),
                              Converter::pixelsToMeters(mLastPreviewSizePx.y) };
            mSelectedBodyId = mWorld->updateSceneObject(mSelectedBodyId);
            mWorld->setSelectedHighlight(mSelectedBodyId);
        }
    }
    else if (mDragMode == DragMode::Move)
    {
        if (mMainCharRowSelected)
        {
            // the character body moved with plain transforms: nothing to
            // commit, the selection overlay already follows the body
        }
        else
        {
            // fix the position: recreate the body (type/size stay). The body id
            // changes on recreation, so capture the fresh one, otherwise the
            // selection outline would die with the old body and the selection
            // mode would look deactivated after releasing the mouse button.
            mSelectedBodyId = mWorld->updateSceneObject(mSelectedBodyId);
            mWorld->setSelectedHighlight(mSelectedBodyId);
        }
    }
    else if (mDragMode == DragMode::Rotate)
    {
        // the rotation was applied live as a body transform; nothing to
        // recreate. Re-express the (world-fixed) pivot in the object's new
        // local frame so the pivot marker follows the rotated object.
        SceneObjectPhysicsParameters* object
            = mWorld->findSceneObject(mSelectedBodyId);
        if (object != nullptr)
        {
            const sf::Vector2f centerPx{
                Converter::metersToPixels(object->mPosition.x),
                Converter::metersToPixels(object->mPosition.y)};
            const float angleRad
                = mWorld->getSceneObjectRotation(mSelectedBodyId);
            mPivotOffsetPx = rotateOffset(mRotateStartPivotPx - centerPx,
                                          -angleRad);
        }
        updatePivotMarker();
    }
    else if (mDragMode == DragMode::MovePivot)
    {
        // the pivot marker is already up to date (moved live)
    }

    // the drag is over: the preview overlay must go away, otherwise a stale
    // orange outline would stay at the last previewed geometry
    mWorld->setResizePreview({}, {}, false);

    // commit the drag to the undo stack when it changed the object (a
    // click without movement produces no entry); pivot drags only move the
    // rotate-mode marker and are not undoable object edits
    if (mDragUndoValid && mDragMode != DragMode::MovePivot && mWorld != nullptr)
    {
        auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
        if (mDragUndoIndex >= 0
            && mDragUndoIndex < static_cast<int>(objects.size()))
        {
            const auto& after = objects.at(mDragUndoIndex);
            if (objectStatesDiffer(mDragUndoSnapshot, after))
            {
                pushUndoEntry({ mSelectedLevel, mDragUndoIndex,
                                UndoEntry::Kind::Edit, mDragUndoSnapshot,
                                after });
            }
        }
        mDragUndoValid = false;
    }

    // refresh the edit fields from the committed object (fresh body id)
    if (mMainCharRowSelected)
    {
        syncCharEditBuffers();
    }
    else
    {
        SceneObjectPhysicsParameters* committed
            = mWorld->findSceneObject(mSelectedBodyId);
        if (committed != nullptr)
        {
            mEditPositionX = Converter::metersToPixels(committed->mPosition.x);
            mEditPositionY = Converter::metersToPixels(committed->mPosition.y);
            mEditWidth = Converter::metersToPixels(committed->mSize.x);
            mEditHeight = Converter::metersToPixels(committed->mSize.y);
            mEditAngle
                = static_cast<float>(Converter::radToDeg(committed->mRotation));
        }
    }

    mDragMode = DragMode::None;
    mDragHandle = -1;
}

void
Editor::exitSelectionMode()
{
    if (not mWorld) { return; }
    mSelectedBodyId = b2_nullBodyId;
    mDragMode = DragMode::None;
    mDragHandle = -1;
    mHoveredBodyId = b2_nullBodyId;
    mWorld->setSelectedHighlight(b2_nullBodyId);
    // withOutline=false also resets the renderer's outline-on-hover state
    mWorld->setHoverHighlight(b2_nullBodyId, false);
    mWorld->setResizePreview({}, {}, false);
    mWorld->setPivotMarker({}, false);
    mWorld->setSelectionStyle(Renderer::SelectionStyle::ResizeHandles);
    mPivotOffsetPx = {};
    mRotateStartMouseAngle = 0.f;
    mRotateStartObjectAngle = 0.f;
}

void
Editor::syncEditBuffersFromSelection()
{
    if (not mWorld || mSelectedIndex < 0) { return; }
    auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
    if (mSelectedIndex >= static_cast<int>(objects.size()))
    {
        mSelectedIndex = static_cast<int>(objects.size()) - 1;
        if (mSelectedIndex < 0) { return; }
    }
    auto& object = objects.at(mSelectedIndex);
    mEditName = object.mName;
    mEditIsDynamic = object.mType == b2_dynamicBody;
    mEditHasPhysics = object.mHasPhysics;
    mEditTextureId = object.mTextureId;
    mEditTileCol = object.mTileCol;
    mEditTileRow = object.mTileRow;
    mEditEntityType = object.mEntityType == EntityType::Climbable ? 1 : 0;
    mEditLayer = object.mLayer;
    mEditPositionX = Converter::metersToPixels(object.mPosition.x);
    mEditPositionY = Converter::metersToPixels(object.mPosition.y);
    mEditWidth = Converter::metersToPixels(object.mSize.x);
    mEditHeight = Converter::metersToPixels(object.mSize.y);
    mEditAngle = static_cast<float>(Converter::radToDeg(object.mRotation));
}

void
Editor::syncCharEditBuffers()
{
    if (not mWorld) { return; }
    const Physics::CharacterPhysicsRect rect = mWorld->getCharacterRect();
    mEditPositionX = Converter::metersToPixels(rect.centerM.x);
    mEditPositionY = Converter::metersToPixels(rect.centerM.y);
    mEditWidth = rect.sizePx.x;
    mEditHeight = rect.sizePx.y;
    mEditAngle = 0.f; // the character body has no rotation
}

/// select + highlight the MAIN CHARACTER pseudo-entry (list/arrow keys)
static void
showMainCharSelection(World *world)
{
    const Physics::CharacterPhysicsRect rect = world->getCharacterRect();
    const sf::Vector2f centerPx{ Converter::metersToPixels(rect.centerM.x),
                                 Converter::metersToPixels(rect.centerM.y) };
    world->setCharacterSelection(centerPx, rect.sizePx, true);
}

void
Editor::handleKey(const sf::Event::KeyPressed& key)
{
    if (not mWorld) { return; }

    // undo/redo shortcuts: Ctrl+Z = undo, Ctrl+Shift+Z / Ctrl+Y = redo;
    // only ignored while a text input is actually active (WantTextInput) -
    // WantCaptureKeyboard would stay on after ANY ImGui interaction (e.g.
    // clicking 'Apply edits') and swallow the shortcuts forever.
    // NOTE: the letter keys are matched via key.code (layout-localized
    // virtual key), NOT via scancode: scancodes are physical positions
    // labeled after US-QWERTY, so on a German QWERTZ keyboard the 'Z' key
    // reports scancode Y - matching scancodes made STRG+Z trigger redo
    // Tab cycles through the tools: None -> Resize -> Rotate -> None
    if (key.code == sf::Keyboard::Key::Tab
        && not key.control && not key.alt
        && not ImGui::GetIO().WantTextInput)
    {
        cycleTool();
        return;
    }

    if (key.control && not ImGui::GetIO().WantTextInput)
    {
        if (key.code == sf::Keyboard::Key::Z)
        {
            if (key.shift) { redo(); } else { undo(); }
            return;
        }
        if (key.code == sf::Keyboard::Key::Y)
        {
            redo();
            return;
        }
    }

    const sf::Keyboard::Scancode scancode = key.scancode;
    int step = 0;
    if (scancode == sf::Keyboard::Scancode::Up) { step = -1; }
    else if (scancode == sf::Keyboard::Scancode::Down) { step = 1; }
    else if (scancode == sf::Keyboard::Scancode::Left) { step = -1; }
    else if (scancode == sf::Keyboard::Scancode::Right) { step = 1; }
    else { return; }

    auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
    if (objects.empty()) { return; }

    mSelectedIndex += step;
    // wrap around both ends; -1 = the MainCharacter pseudo-entry
    const int maxIndex = static_cast<int>(objects.size()) - 1;
    if (mSelectedIndex < -1)
    {
        mSelectedIndex = maxIndex;
    }
    else if (mSelectedIndex > maxIndex)
    {
        mSelectedIndex = -1;
    }

    mMainCharRowSelected = mSelectedIndex == -1;
    mLastSelectedIndex = -1;
    if (mMainCharRowSelected)
    {
        // main character: no physics-body id in the scene-objects list; the
        // selection overlay is drawn from the live character rect instead
        mSelectedBodyId = b2_nullBodyId;
        mWorld->setSelectedHighlight(b2_nullBodyId);
        syncCharEditBuffers();
        showMainCharSelection(mWorld);
        return;
    }

    syncEditBuffersFromSelection();
    // also mark the object inside the running level
    mSelectedBodyId
        = objects.at(mSelectedIndex).mPhysicsBodyId;
    mPivotOffsetPx = {};
    mWorld->setSelectedHighlight(mSelectedBodyId);
    if (mActiveTool == Tool::Rotate)
    {
        mWorld->setSelectionStyle(Renderer::SelectionStyle::RotateMarkers);
    }
    updatePivotMarker();
    mWorld->setCharacterSelection({}, {}, false);
}

/// ---- undo / redo -------------------------------------------------------

bool
Editor::objectStatesDiffer(const SceneObjectPhysicsParameters& lhs,
                           const SceneObjectPhysicsParameters& rhs) const
{
    return lhs.mPosition.x != rhs.mPosition.x
        || lhs.mPosition.y != rhs.mPosition.y
        || lhs.mSize.x != rhs.mSize.x
        || lhs.mSize.y != rhs.mSize.y
        || lhs.mRotation != rhs.mRotation
        || lhs.mName != rhs.mName
        || lhs.mType != rhs.mType
        || lhs.mTextureId != rhs.mTextureId
        || lhs.mEntityType != rhs.mEntityType
        || lhs.mLayer != rhs.mLayer
        || lhs.mHasPhysics != rhs.mHasPhysics
        || lhs.mTextureId != rhs.mTextureId
        || lhs.mTileCol != rhs.mTileCol
        || lhs.mTileRow != rhs.mTileRow;
}

void
Editor::pushUndoEntry(const UndoEntry& entry)
{
    mUndoStack.push_back(entry);
    while (mMaxUndoSteps > 0
           && static_cast<int>(mUndoStack.size()) > mMaxUndoSteps)
    {
        mUndoStack.pop_front();
    }
    mRedoStack.clear();
}

bool
Editor::applyUndoState(const UndoEntry& entry, bool redoState)
{
    if (mWorld == nullptr) { return false; }

    // structural steps: undo of a delete re-inserts the object, undo of an
    // add removes it again (redo inverts each)
    if (entry.kind == UndoEntry::Kind::Delete)
    {
        if (redoState) { return removeSceneObjectAtIndex(entry.level, entry.index); }
        return reinsertSceneObject(entry.level, entry.index, entry.before);
    }
    if (entry.kind == UndoEntry::Kind::Add)
    {
        if (redoState)
        {
            return reinsertSceneObject(entry.level, entry.index, entry.after);
        }
        return removeSceneObjectAtIndex(entry.level, entry.index);
    }

    // tile strokes: one entry for the whole drag (place n objects / erase
    // n objects); playback keeps the indices valid in each direction
    if (entry.kind == UndoEntry::Kind::StrokeAdd)
    {
        // replace-mode strokes are mixed: the placed objects go away and
        // the replaced (removed) ones come back; the placed tiles carry
        // unique names (tileset + tile coords), so undo finds them by
        // name + position instead of by (stale) row indices
        if (not redoState)
        {
            auto& objects = mWorld->getSceneObjects().at(entry.level);
            for (int k = static_cast<int>(entry.strokeParams.size()) - 1;
                 k >= 0; --k)
            {
                const auto& params = entry.strokeParams.at(k);
                for (int i = static_cast<int>(objects.size()) - 1; i >= 0;
                     --i)
                {
                    const auto& object = objects.at(i);
                    const float ox =
                        Converter::metersToPixels(object.mPosition.x);
                    const float oy =
                        Converter::metersToPixels(object.mPosition.y);
                    const float px =
                        Converter::metersToPixels(params.mPosition.x);
                    const float py =
                        Converter::metersToPixels(params.mPosition.y);
                    if (object.mName == params.mName
                        and std::abs(ox - px) < 0.1f
                        and std::abs(oy - py) < 0.1f)
                    {
                        removeSceneObjectAtIndex(entry.level, i);
                        break;
                    }
                }
            }
            // the replaced objects return in reverse removal order (each
            // recorded index is valid again in the state of its time)
            for (int k = static_cast<int>(entry.strokeRemovedIndices.size())
                     - 1;
                 k >= 0; --k)
            {
                reinsertSceneObject(entry.level,
                                    entry.strokeRemovedIndices.at(k),
                                    entry.strokeRemovedParams.at(k));
            }
            return true;
        }
        // redo: replay the original removal order (its indices are valid
        // step by step, the list is back in its pre-stroke order), then
        // re-append the placed tiles at the end in stroke order
        for (std::size_t k = 0;
             k < entry.strokeRemovedIndices.size(); ++k)
        {
            removeSceneObjectAtIndex(entry.level,
                                     entry.strokeRemovedIndices.at(k));
        }
        auto& objects = mWorld->getSceneObjects().at(entry.level);
        for (std::size_t k = 0; k < entry.strokeParams.size(); ++k)
        {
            reinsertSceneObject(entry.level,
                                static_cast<int>(objects.size()),
                                entry.strokeParams.at(k));
        }
        return true;
    }
    if (entry.kind == UndoEntry::Kind::StrokeDelete)
    {
        if (redoState)
        {
            // redo: replay the original removal order (its indices match
            // the list state of their time step by step)
            for (std::size_t k = 0; k < entry.strokeIndices.size(); ++k)
            {
                removeSceneObjectAtIndex(entry.level,
                                         entry.strokeIndices.at(k));
            }
            return true;
        }
        // undo: re-insert in reverse removal order (each index is valid
        // again in the list state it was recorded in)
        for (int k = static_cast<int>(entry.strokeIndices.size()) - 1;
             k >= 0; --k)
        {
            reinsertSceneObject(entry.level, entry.strokeIndices.at(k),
                                entry.strokeParams.at(k));
        }
        return true;
    }

    auto& objects = mWorld->getSceneObjects().at(entry.level);
    if (entry.index < 0 || entry.index >= static_cast<int>(objects.size()))
    {
        fmt::print("LevelEditor: undo entry no longer matches the level data.\n");
        return false;
    }
    auto& object = objects.at(entry.index);
    const SceneObjectPhysicsParameters& state
        = redoState ? entry.after : entry.before;
    object.mPosition = state.mPosition;
    object.mSize = state.mSize;
    object.mRotation = state.mRotation;
    object.mName = state.mName;
    object.mType = state.mType;
    object.mTextureId = state.mTextureId;
    object.mEntityType = state.mEntityType;
    object.mLayer = state.mLayer;

    // body recreation keeps physics (and the renderer) in sync; updateSceneObject
    // writes the fresh body id back into the object
    const b2BodyId newBodyId = mWorld->updateSceneObject(object.mPhysicsBodyId);
    if (bodyIdsEqual(mSelectedBodyId, object.mPhysicsBodyId))
    {
        mSelectedBodyId = newBodyId;
        mWorld->setSelectedHighlight(newBodyId);
        updatePivotMarker();
    }
    if (mSelectedLevel == entry.level && mSelectedIndex == entry.index
        && not mMainCharRowSelected)
    {
        syncEditBuffersFromSelection();
    }
    return true;
}

bool
Editor::removeSceneObjectAtIndex(ELevel level, int index)
{
    if (mWorld == nullptr) { return false; }
    auto& objects = mWorld->getSceneObjects().at(level);
    if (index < 0 || index >= static_cast<int>(objects.size()))
    {
        fmt::print("LevelEditor: no object at index {} to remove.\n", index);
        return false;
    }
    const b2BodyId bodyId = objects.at(index).mPhysicsBodyId;
    if (bodyIdsEqual(mSelectedBodyId, bodyId))
    {
        mSelectedBodyId = b2_nullBodyId;
        mWorld->setSelectedHighlight(b2_nullBodyId);
        updatePivotMarker();
    }
    if (mSelectedLevel == level && mSelectedIndex == index
        && not mMainCharRowSelected)
    {
        mSelectedIndex = static_cast<int>(objects.size()) - 2;
        mLastSelectedIndex = -1;
        syncEditBuffersFromSelection();
    }
    mWorld->removeSceneObject(bodyId);
    adjustUndoIndicesForRemoval(level, index);
    return true;
}

bool
Editor::reinsertSceneObject(ELevel level, int index,
                            const SceneObjectPhysicsParameters& params)
{
    if (mWorld == nullptr) { return false; }
    auto& objects = mWorld->getSceneObjects().at(level);
    if (index < 0 || index > static_cast<int>(objects.size()))
    {
        fmt::print("LevelEditor: invalid insert index {}.\n", index);
        return false;
    }
    // addSceneObject appends + creates the physics body; afterwards the
    // row is moved back to its original position in the list
    SceneObjectPhysicsParameters restored{ params };
    restored.mPhysicsBodyId = b2_nullBodyId;
    mWorld->addSceneObject(level, std::move(restored));
    const int appendedIndex = static_cast<int>(objects.size()) - 1;
    std::rotate(objects.begin() + index,
                objects.begin() + appendedIndex, objects.end());
    adjustUndoIndicesForInsertion(level, index);
    if (mSelectedLevel == level && mSelectedIndex == index
        && not mMainCharRowSelected)
    {
        mSelectedBodyId = objects.at(index).mPhysicsBodyId;
        mWorld->setSelectedHighlight(mSelectedBodyId);
        updatePivotMarker();
        syncEditBuffersFromSelection();
    }
    return true;
}

void
Editor::adjustUndoIndicesForRemoval(ELevel level, int index)
{
    for (auto* stack : { &mUndoStack, &mRedoStack })
    {
        std::deque<UndoEntry> kept;
        for (auto& entry : *stack)
        {
            if (entry.level != level)
            {
                kept.push_back(std::move(entry));
                continue;
            }
            // entries of the removed object die with it: after a later
            // re-insert they would restore fields onto the wrong object
            if (entry.index == index) { continue; }
            if (entry.index > index) { --entry.index; }
            kept.push_back(std::move(entry));
        }
        stack->swap(kept);
    }
}

void
Editor::adjustUndoIndicesForInsertion(ELevel level, int index)
{
    for (auto* stack : { &mUndoStack, &mRedoStack })
    {
        for (auto& entry : *stack)
        {
            if (entry.level == level && entry.index >= index) { ++entry.index; }
        }
    }
}

void
Editor::undo()
{
    if (mUndoStack.empty())
    {
        fmt::print("LevelEditor: nothing to undo.\n");
        return;
    }
    UndoEntry entry = mUndoStack.back();
    mUndoStack.pop_back();
    if (applyUndoState(entry, false))
    {
        mRedoStack.push_back(entry);
        fmt::print("LevelEditor: undo ({} steps left).\n",
                   mUndoStack.size());
    }
}

void
Editor::redo()
{
    if (mRedoStack.empty())
    {
        fmt::print("LevelEditor: nothing to redo.\n");
        return;
    }
    UndoEntry entry = mRedoStack.back();
    mRedoStack.pop_back();
    if (applyUndoState(entry, true))
    {
        mUndoStack.push_back(entry);
        fmt::print("LevelEditor: redo ({} steps left).\n",
                   mRedoStack.size());
    }
}

/// ---- rename window / editor settings ------------------------------------

void
Editor::beginRename(int index)
{
    if (not mWorld || index < 0) { return; }
    auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
    if (index >= static_cast<int>(objects.size())) { return; }
    mRenameIndex = index;
    std::snprintf(mRenameBuffer, sizeof(mRenameBuffer), "%s",
                  objects.at(index).mName.c_str());
    mRenameActive = true;
}

void
Editor::drawCreateLevelWindow()
{
    if (not mShowCreateLevel) { return; }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::Begin("Create level", &mShowCreateLevel,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        bool create = ImGui::InputText("Name##newlevel", mNewLevelName,
                                       sizeof(mNewLevelName),
                                       ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Create")) { create = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { mShowCreateLevel = false; }
        ImGui::TextWrapped("Creates levelN.json in the next free slot: an "
                           "empty level with one floor (full width, bottom) "
                           "and the character in the middle of the screen.");

        if (create && mWorld != nullptr)
        {
            if (createLevelFromName(mNewLevelName))
            {
                mShowCreateLevel = false;
                mNewLevelName[0] = '\0';
            }
        }
    }
    ImGui::End();
}

bool
Editor::createLevelFromName(const std::string& name)
{
    if (mWorld == nullptr or name.empty())
    {
        fmt::print("LevelEditor: create level needs a name.\n");
        return false;
    }

    // first free slot: no level data loaded for this ELevel
    ELevel target = ELevel::None;
    for (auto level : ELevel::_values())
    {
        if (level == +ELevel::None) { continue; }
        if (not mWorld->getSceneObjects().contains(level))
        {
            target = level;
            break;
        }
    }
    if (target == +ELevel::None)
    {
        fmt::print("LevelEditor: no free level slot (all slots used).\n");
        return false;
    }

    // floor like level2's: full canvas width, at the bottom
    SceneObjectPhysicsParameters floor{
        { 320.f, 360.f },
        { 640.f, 9.f },
        "floor",
        b2BodyType::b2_staticBody,
        "Grass",
        EntityType::None,
        1,
        0.f,
        true,
        -1,
        -1
    };
    mWorld->addSceneObject(target, std::move(floor));
    mWorld->getLevelNames()[target] = name;

    // character to the middle of the screen
    mWorld->moveMainCharacterToPx({ 320.f, 180.f });

    // persist the new level right away (name + current parallax values)
    const RyuLevel::ParallaxFactors parallax{ mParallax[0], mParallax[1],
                                              mParallax[2] };
    RyuLevel::saveLevelData(target, mWorld->getSceneObjects().at(target),
                            "assets/levels", &parallax, &name);

    // rebuild the level (physics + renderer) and select it
    switchLevel(target);
    fmt::print("LevelEditor: created level '{}' as {}.\n", name,
               target._to_string());
    return true;
}

void
Editor::drawRenameWindow()
{
    if (not mRenameActive) { return; }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing,
                            ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(320.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::Begin("Rename object", &mRenameActive,
                     ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        bool apply = ImGui::InputText("Name##rename", mRenameBuffer,
                                      sizeof(mRenameBuffer),
                                      ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Apply")) { apply = true; }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { mRenameActive = false; }
        ImGui::TextWrapped("Double-click on an object in the list to rename "
                           "it; Enter or Apply commits the change.");

        if (apply && mWorld != nullptr)
        {
            auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
            if (mRenameIndex >= 0
                && mRenameIndex < static_cast<int>(objects.size()))
            {
                auto& object = objects.at(mRenameIndex);
                UndoEntry entry{ mSelectedLevel, mRenameIndex,
                                 UndoEntry::Kind::Edit, object, object };
                object.mName = mRenameBuffer;
                entry.after = object;
                if (mSelectedIndex == mRenameIndex)
                {
                    mEditName = mRenameBuffer;
                    mLastSelectedIndex = -1; // refresh remaining edit buffers
                }
                pushUndoEntry(entry);
            }
            mRenameActive = false;
        }
    }
    ImGui::End();
}

void
Editor::saveEditorSettings()
{
    std::ofstream oSet(mSettingsPath);
    if (not oSet.is_open())
    {
        fmt::print("LevelEditor: could not open {} for writing.\n",
                   mSettingsPath);
        return;
    }
    nlohmann::json settings = { {"undoSteps", mMaxUndoSteps} };
    oSet << settings.dump(2) << "\n";
    fmt::print("LevelEditor: saved editor-settings to {}\n",
               mSettingsPath);
}

void
Editor::loadEditorSettings()
{
    std::ifstream iSet(mSettingsPath);
    if (not iSet.is_open()) { return; }
    try
    {
        nlohmann::json settings = nlohmann::json::parse(iSet);
        if (settings.contains("undoSteps"))
        {
            mMaxUndoSteps = std::clamp(settings["undoSteps"].get<int>(), 1, 500);
        }
        fmt::print("LevelEditor: loaded editor-settings from {}\n",
                   mSettingsPath);
    }
    catch (const std::exception& e)
    {
        fmt::print("LevelEditor: could not read editor-settings {}: {}\n",
                   mSettingsPath, e.what());
    }
}

void
Editor::drawEditorSettings()
{
    if (not mShowEditorSettings) { return; }
    ImGui::SetNextWindowPos(ImVec2(360.f, 120.f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(340.f, 0.f), ImGuiCond_Appearing);
    if (ImGui::Begin("Level Editor Settings", &mShowEditorSettings,
                     ImGuiWindowFlags_NoCollapse))
    {
        // how many edits are kept on the undo stack; applied live
        if (ImGui::InputInt("Undo steps", &mMaxUndoSteps, 1, 10))
        {
            mMaxUndoSteps = std::clamp(mMaxUndoSteps, 1, 500);
            while (static_cast<int>(mUndoStack.size()) > mMaxUndoSteps)
            {
                mUndoStack.pop_front();
            }
            while (static_cast<int>(mRedoStack.size()) > mMaxUndoSteps)
            {
                mRedoStack.pop_front();
            }
        }
        ImGui::TextWrapped("STRG+Z undoes the last object edit, "
                           "STRG+SHIFT+Z (or STRG+Y) redoes it; TAB cycles "
                           "the tools. Up to 'Undo steps' edits are "
                           "remembered.");
        ImGui::Separator();
        ImGui::SetNextItemWidth(215);
        ImGui::InputText("Settings file", mSettingsPath,
                         IM_ARRAYSIZE(mSettingsPath));
        ImGui::SameLine();
        if (ImGui::Button("Save as default")) { saveEditorSettings(); }
    }
    ImGui::End();
}

/// ---- tile placement tools (TilePlacer / TileRemover) -------------------

sf::Vector2i
Editor::tileCellAt(const sf::Vector2f& posPx) const
{
    const float cellW = static_cast<float>(std::max(mGridCellWidth, 1));
    const float cellH = static_cast<float>(std::max(mGridCellHeight, 1));
    return { static_cast<int>(std::floor(posPx.x / cellW)),
             static_cast<int>(std::floor(posPx.y / cellH)) };
}

sf::Vector2f
Editor::tileCellCenterPx(const sf::Vector2i& cell) const
{
    const float cellW = static_cast<float>(std::max(mGridCellWidth, 1));
    const float cellH = static_cast<float>(std::max(mGridCellHeight, 1));
    return { (static_cast<float>(cell.x) + 0.5f) * cellW,
             (static_cast<float>(cell.y) + 0.5f) * cellH };
}

bool
Editor::tileCellOccupied(const sf::Vector2f& centerPx) const
{
    if (mWorld == nullptr) { return false; }
    const auto& objects =
        mWorld->getSceneObjects().at(mWorld->getActiveLevel());
    const float cellW = static_cast<float>(std::max(mGridCellWidth, 1));
    const float cellH = static_cast<float>(std::max(mGridCellHeight, 1));
    for (const auto& object : objects)
    {
        const float ox = Converter::metersToPixels(object.mPosition.x);
        const float oy = Converter::metersToPixels(object.mPosition.y);
        const float hw = Converter::metersToPixels(object.mSize.x) / 2.f;
        const float hh = Converter::metersToPixels(object.mSize.y) / 2.f;
        // small negative slack so exact cell adjacency is no overlap
        if (std::abs(ox - centerPx.x) < hw + cellW / 2.f - 0.5f
            and std::abs(oy - centerPx.y) < hh + cellH / 2.f - 0.5f)
        {
            return true;
        }
    }
    return false;
}

void
Editor::syncTileGridToBrush()
{
    if (mWorld == nullptr or mEditTileCol < 0 or mEditTextureId.empty())
    {
        return;
    }
    // the brush tile dictates the grid: painting snaps to the tile size
    auto& registry = mWorld->getTextureRegistry();
    if (registry.tileset(mEditTextureId) == nullptr) { return; }
    const sf::IntRect rect = registry.textureRect(mEditTextureId,
                                                 mEditTileCol, mEditTileRow);
    if (rect.size.x <= 0 or rect.size.y <= 0) { return; }
    mGridCellWidth = rect.size.x;
    mGridCellHeight = rect.size.y;
    mShowGrid = true;
    // same overlay update as the grid controls (cell size + color + show)
    const sf::Color color = mGridColorIndex == 1 ? sf::Color(0, 255, 0, 90)
                                                 : sf::Color(255, 0, 0, 90);
    mWorld->setGridOverlay(true, { static_cast<float>(mGridCellWidth),
                                   static_cast<float>(mGridCellHeight) },
                           color);
}

void
Editor::beginTileStroke(bool erase, const sf::Vector2f& posPx)
{
    if (mWorld == nullptr) { return; }
    if (mActiveTool == Tool::TileRemover) { erase = true; }
    if (not erase)
    {
        // painting requires a loaded tileset tile as the brush
        auto& registry = mWorld->getTextureRegistry();
        if (mEditTileCol < 0 or registry.texture(mEditTextureId) == nullptr
            or registry.tileset(mEditTextureId) == nullptr)
        {
            fmt::print("LevelEditor: tile placer needs a picked tileset tile "
                       "as the brush (texture picker).\n");
            return;
        }
        syncTileGridToBrush();
    }
    mTileStrokeActive = true;
    mTileStrokeErase = erase;
    mTileStrokePrevPosPx = posPx;
    mTileStrokePrevValid = false;
    mTileStrokeIndices.clear();
    mTileStrokePlacedParams.clear();
    mTileStrokeRemoved.clear();
    updateTileStroke(posPx);
}

void
Editor::updateTileStroke(const sf::Vector2f& posPx)
{
    if (not mTileStrokeActive) { return; }
    const auto paint = [this](const sf::Vector2f& pos)
    {
        const sf::Vector2f center = tileCellCenterPx(tileCellAt(pos));
        if (mTileStrokeErase) { eraseTileAt(center); }
        else { placeTileAt(center); }
    };
    if (not mTileStrokePrevValid)
    {
        paint(posPx);
    }
    else
    {
        // interpolate: a fast drag must not leave gaps between the
        // sampled positions; walk the line in half-cell steps
        const sf::Vector2f delta = posPx - mTileStrokePrevPosPx;
        const float dist = std::sqrt(delta.x * delta.x + delta.y * delta.y);
        const float step = std::min(
            static_cast<float>(std::max(mGridCellWidth, 1)),
            static_cast<float>(std::max(mGridCellHeight, 1))) / 2.f;
        const int steps = dist > step ? static_cast<int>(dist / step) : 1;
        for (int k = 1; k <= steps; ++k)
        {
            paint(mTileStrokePrevPosPx
                  + delta * (static_cast<float>(k) / steps));
        }
    }
    mTileStrokePrevPosPx = posPx;
    mTileStrokePrevValid = true;
}

void
Editor::placeTileAt(const sf::Vector2f& cellCenterPx)
{
    if (mWorld == nullptr) { return; }
    if (tileCellOccupied(cellCenterPx))
    {
        if (not mReplaceTiles) { return; } // off: occupied cells are skipped
        // replace: only tile-sized objects of the cell are removed first;
        // big manually built geometry protects the cell (floor stays)
        eraseTileAt(cellCenterPx);
        if (tileCellOccupied(cellCenterPx)) { return; }
    }
    auto& registry = mWorld->getTextureRegistry();
    const sf::IntRect rect = registry.textureRect(mEditTextureId,
                                                 mEditTileCol, mEditTileRow);

    // unique, equally wide names: the next free counter suffix of all
    // tile-placed objects of this level (_0000, _0001, ...); tiles painted
    // in an earlier session keep counting on from their stored suffixes
    int maxCounter = -1;
    const ELevel level = mWorld->getActiveLevel();
    for (const auto& existing
         : mWorld->getSceneObjects().at(level))
    {
        const std::string& name = existing.mName;
        const auto lastUnderscore = name.rfind('_');
        if (lastUnderscore == std::string::npos
            or name.size() - lastUnderscore - 1 != 4)
        {
            continue;
        }
        const std::string suffix = name.substr(lastUnderscore + 1);
        if (not std::all_of(suffix.begin(), suffix.end(), ::isdigit))
        {
            continue;
        }
        maxCounter = std::max(maxCounter, std::stoi(suffix));
    }

    SceneObjectPhysicsParameters object{
        { cellCenterPx.x, cellCenterPx.y },
        { static_cast<float>(rect.size.x), static_cast<float>(rect.size.y) },
        fmt::format("tile_{}_{}_{}_{:04d}", mEditTextureId, mEditTileCol,
                    mEditTileRow, maxCounter + 1),
        b2BodyType::b2_staticBody,
        mEditTextureId,
        EntityType::None,
        mEditLayer,
        0.f,
        mEditHasPhysics,
        mEditTileCol,
        mEditTileRow
    };
    auto& added = mWorld->addSceneObject(level, std::move(object));
    auto& objects = mWorld->getSceneObjects().at(level);
    mTileStrokePlacedParams.push_back(added);
    mTileStrokeIndices.push_back(static_cast<int>(objects.size()) - 1);
}

void
Editor::eraseTileAt(const sf::Vector2f& cellCenterPx)
{
    if (mWorld == nullptr) { return; }
    const ELevel level = mWorld->getActiveLevel();
    auto& objects = mWorld->getSceneObjects().at(level);
    const float cellW = static_cast<float>(std::max(mGridCellWidth, 1));
    const float cellH = static_cast<float>(std::max(mGridCellHeight, 1));
    std::vector<int> candidates;
    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        const auto& object = objects.at(i);
        const float ox = Converter::metersToPixels(object.mPosition.x);
        const float oy = Converter::metersToPixels(object.mPosition.y);
        const float ow = Converter::metersToPixels(object.mSize.x);
        const float oh = Converter::metersToPixels(object.mSize.y);
        // only tile-sized objects whose center lies in this cell are
        // erasable: big manually built geometry (floors, walls) survives
        const bool inCell = std::abs(ox - cellCenterPx.x) <= cellW / 2.f
            and std::abs(oy - cellCenterPx.y) <= cellH / 2.f;
        const bool tileSized = std::abs(ow - cellW) <= 1.f
            and std::abs(oh - cellH) <= 1.f;
        if (inCell and tileSized) { candidates.push_back(i); }
    }
    // remove descending: earlier recorded indices stay valid
    for (int k = static_cast<int>(candidates.size()) - 1; k >= 0; --k)
    {
        const int index = candidates.at(k);
        mTileStrokeRemoved.push_back({ index, objects.at(index) });
        removeSceneObjectAtIndex(level, index);
    }
}

void
Editor::endTileStroke()
{
    if (not mTileStrokeActive) { return; }
    mTileStrokeActive = false;
    mTileStrokePrevValid = false;
    if (mWorld == nullptr)
    {
        mTileStrokeIndices.clear();
        mTileStrokePlacedParams.clear();
        mTileStrokeRemoved.clear();
        return;
    }

    UndoEntry entry{ mWorld->getActiveLevel(), -1,
                     UndoEntry::Kind::StrokeAdd, {}, {} };
    if (mTileStrokeErase)
    {
        if (mTileStrokeRemoved.empty()) { return; }
        entry.kind = UndoEntry::Kind::StrokeDelete;
        entry.strokeIndices.reserve(mTileStrokeRemoved.size());
        entry.strokeParams.reserve(mTileStrokeRemoved.size());
        for (const auto& removed : mTileStrokeRemoved)
        {
            entry.strokeIndices.push_back(removed.index);
            entry.strokeParams.push_back(removed.params);
        }
    }
    else
    {
        if (mTileStrokePlacedParams.empty()) { return; }
        entry.strokeIndices = mTileStrokeIndices;
        entry.strokeParams = mTileStrokePlacedParams;
        // replace-mode strokes may have removed objects on the way
        for (const auto& removed : mTileStrokeRemoved)
        {
            entry.strokeRemovedIndices.push_back(removed.index);
            entry.strokeRemovedParams.push_back(removed.params);
        }
    }
    // one entry per stroke: a whole drag is undone with a single Ctrl+Z
    pushUndoEntry(entry);
    fmt::print("LevelEditor: tile stroke: {} objects {}.\n",
               entry.strokeIndices.size(),
               mTileStrokeErase ? "erased" : "placed");
}

void
Editor::drawToolSidebar()
{
    // tools-sidebar (photoshop-style), left edge of the game view
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(110.f, 0.f), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                             | ImGuiWindowFlags_AlwaysAutoResize
                             | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("##levelEditorTools", nullptr, flags);

    ImGui::Text("Tools");

    auto toolButton = [this](const char *label, Tool tool)
    {
        const bool active = mActiveTool == tool;
        if (active)
        {
            ImGui::PushStyleColor(ImGuiCol_Button,
                                  ImVec4(1.f, 0.55f, 0.f, 0.85f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.f, 0.f, 0.f, 1.f));
        }
        if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.f)))
        {
            switchTool(active ? Tool::None : tool);
        }
        if (active) { ImGui::PopStyleColor(2); }
    };

    toolButton("Resize", Tool::Resize);
    toolButton("Rotate", Tool::Rotate);
    toolButton("Tile paint", Tool::TilePlacer);
    toolButton("Tile erase", Tool::TileRemover);
    ImGui::Separator();
    ImGui::BeginDisabled();
    toolButton("Edit", Tool::Edit);
    ImGui::EndDisabled();
    ImGui::Separator();

    ImGui::End();
}

void
Editor::drawGridControls()
{
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Grid Mode"))
    {
        // every change pushes the complete overlay state to the renderer so
        // the grid updates live (cell size, color, on/off)
        auto updateGrid = [&]()
        {
            if (mWorld == nullptr) { return; }
            const sf::Vector2f cellSizePx{
                static_cast<float>(std::max(mGridCellWidth, 4)),
                static_cast<float>(std::max(mGridCellHeight, 4))};
            const sf::Color color = mGridColorIndex == 1
                                        ? sf::Color(0, 255, 0, 90)
                                        : sf::Color(255, 0, 0, 90);
            mWorld->setGridOverlay(mShowGrid, cellSizePx, color);
        };

        if (ImGui::Checkbox("Enable grid", &mShowGrid)) { updateGrid(); }
        if (ImGui::InputInt("Cell width##grid", &mGridCellWidth, 4))
        {
            mGridCellWidth = std::max(mGridCellWidth, 4);
            updateGrid();
        }
        if (ImGui::InputInt("Cell height##grid", &mGridCellHeight, 4))
        {
            mGridCellHeight = std::max(mGridCellHeight, 4);
            updateGrid();
        }
        const char *gridColorNames[2] = { "Red", "Green" };
        if (ImGui::Combo("Grid color", &mGridColorIndex,
                         gridColorNames, 2))
        {
            updateGrid();
        }
        ImGui::TextWrapped("Later used to create tiles/tilesets for "
                           "texturizing the sceneobjects.");
        
        // Snapping controls
        ImGui::Separator();
        ImGui::Text("Snapping");
        
        // Vertical snapping (mutually exclusive: Up or Down or None)
        ImGui::Text("Vertical:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Up##snapV", mSnapUp && !mSnapDown))
        {
            setSnapDirectionVertical(true);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Down##snapV", mSnapDown && !mSnapUp))
        {
            setSnapDirectionVertical(false);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("None##snapV", !mSnapUp && !mSnapDown))
        {
            mSnapUp = false;
            mSnapDown = false;
        }
        
        // Horizontal snapping (mutually exclusive: Left or Right or None)
        ImGui::Text("Horizontal:");
        ImGui::SameLine();
        if (ImGui::RadioButton("Left##snapH", mSnapLeft && !mSnapRight))
        {
            setSnapDirectionHorizontal(true);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("Right##snapH", mSnapRight && !mSnapLeft))
        {
            setSnapDirectionHorizontal(false);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton("None##snapH", !mSnapLeft && !mSnapRight))
        {
            mSnapLeft = false;
            mSnapRight = false;
        }
        
        ImGui::TextWrapped("When active and moving objects in selection mode, "
                           "objects will snap to the nearest grid line based on the selected direction.");
    }
}


void
Editor::drawParallaxControls()
{
    ImGui::Separator();
    if (not ImGui::CollapsingHeader("Parallax (per layer)")) { return; }
    if (mWorld == nullptr) { return; }

    // buffers are synced in switchLevel; sliders write the live values to
    // the renderer, save writes them into the level JSON root
    static const char* layerNames[3] = { "Background", "Ground1 (walk plane)",
                                         "Foreground" };
    for (int i = 0; i < 3; ++i)
    {
        if (ImGui::DragFloat(layerNames[i], &mParallax[i], 0.005f, 0.f, 2.f,
                             "%.2f"))
        {
            mWorld->setLayerParallax(static_cast<std::size_t>(i),
                                     mParallax[i]);
        }
    }
    if (ImGui::Button("Reset defaults##parallax"))
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            mParallax[i] = RyuLevel::kParallaxDefaults.at(i);
            mWorld->setLayerParallax(i, mParallax[i]);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("1.00 = glued to the world");

    // the levels fit the fixed canvas so the camera normally never moves;
    // this preview pan shifts the view so the layers visibly separate
    if (ImGui::DragFloat2("Camera preview offset", &mCameraPreviewOffsetPx.x,
                         1.f, -320.f, 320.f, "%.0f"))
    {
        mWorld->setWorldViewCenterPx({ 320.f + mCameraPreviewOffsetPx.x,
                                       180.f + mCameraPreviewOffsetPx.y });
    }
    ImGui::TextWrapped("Pan the camera preview to see the layers separate; "
                       "the pan resets when the editor closes. 'Save level' "
                       "writes the factors into the level JSON.");
}


void
Editor::setSnapDirectionVertical(bool up)
{
    mSnapUp = up;
    mSnapDown = !up;
}

void
Editor::setSnapDirectionHorizontal(bool left)
{
    mSnapLeft = left;
    mSnapRight = !left;
}

/// Snap the position to the nearest grid line based on active snap directions
/// @param positionPx the center position in pixels (in/out)
/// @param sizePx the object size in pixels
void
Editor::applySnapping(sf::Vector2f& positionPx, const sf::Vector2f& sizePx) const
{
    if (!mSnapUp && !mSnapDown && !mSnapLeft && !mSnapRight)
    {
        return; // No snapping direction active
    }

    const float cellW = static_cast<float>(std::max(mGridCellWidth, 1));
    const float cellH = static_cast<float>(std::max(mGridCellHeight, 1));

    // Calculate object edges in pixel coordinates
    const sf::Vector2f halfSize = sizePx / 2.f;
    const float leftEdge = positionPx.x - halfSize.x;
    const float rightEdge = positionPx.x + halfSize.x;
    const float topEdge = positionPx.y - halfSize.y;
    const float bottomEdge = positionPx.y + halfSize.y;

    // Snap horizontal (Left/Right)
    if (mSnapLeft)
    {
        // Snap left edge to nearest grid line
        const float snapX = std::round(leftEdge / cellW) * cellW;
        positionPx.x = snapX + halfSize.x;
    }
    else if (mSnapRight)
    {
        // Snap right edge to nearest grid line
        const float snapX = std::round(rightEdge / cellW) * cellW;
        positionPx.x = snapX - halfSize.x;
    }

    // Snap vertical (Up/Down)
    if (mSnapUp)
    {
        // Snap top edge to nearest grid line
        const float snapY = std::round(topEdge / cellH) * cellH;
        positionPx.y = snapY + halfSize.y;
    }
    else if (mSnapDown)
    {
        // Snap bottom edge to nearest grid line
        const float snapY = std::round(bottomEdge / cellH) * cellH;
        positionPx.y = snapY - halfSize.y;
    }
}

void
Editor::cycleTool()
{
    // Tab cycles the tools; switchTool keeps the selection/highlight state
    // consistent (selecting the list's object when entering a mode)
    switch (mActiveTool)
    {
    case Tool::None: switchTool(Tool::Resize); break;
    case Tool::Resize: switchTool(Tool::Rotate); break;
    case Tool::Rotate: switchTool(Tool::TilePlacer); break;
    case Tool::TilePlacer: switchTool(Tool::TileRemover); break;
    default: switchTool(Tool::None); break;
    }
}

void
Editor::selectObjectAt(const sf::Vector2f& mouseWorldPosPx)
{
    const ELevel activeLevel = mWorld->getActiveLevel();
    // getSceneObjectAt hit-tests the object rects in PIXELS
    const b2Vec2 positionPx{ mouseWorldPosPx.x, mouseWorldPosPx.y };
    const b2BodyId hit = mWorld->getSceneObjectAt(activeLevel, positionPx);

    mMainCharRowSelected = false;
    mWorld->setCharacterSelection({}, {}, false);

    if (B2_IS_NULL(hit))
    {
        // click on empty space clears the selection
        mSelectedIndex = -1;
        mLastSelectedIndex = -1;
        mSelectedBodyId = b2_nullBodyId;
        mWorld->setSelectedHighlight(b2_nullBodyId);
        return;
    }

    // keep the object list and the edit buffers in sync with the click
    auto& objects = mWorld->getSceneObjects().at(activeLevel);
    mSelectedIndex = -1;
    for (int i = 0; i < static_cast<int>(objects.size()); ++i)
    {
        if (bodyIdsEqual(objects.at(i).mPhysicsBodyId, hit))
        {
            mSelectedIndex = i;
            break;
        }
    }
    mLastSelectedIndex = -1;
    mSelectedBodyId = hit;
    mPivotOffsetPx = {};
    mWorld->setSelectedHighlight(hit);
    // no transform tool active: plain selection look (no handles/markers)
    mWorld->setSelectionStyle(Renderer::SelectionStyle::FillOnly);
    syncEditBuffersFromSelection();
}

void
Editor::openToolContextMenu(const sf::Vector2i& screenPosPx)
{
    mContextMenuPosX = screenPosPx.x;
    mContextMenuPosY = screenPosPx.y;
    mShowToolContextMenu = true;
}

void
Editor::drawToolContextMenu()
{
    if (not mShowToolContextMenu) { return; }

    ImGui::SetNextWindowPos(
        ImVec2(static_cast<float>(mContextMenuPosX),
               static_cast<float>(mContextMenuPosY)),
        ImGuiCond_Appearing);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar
                                   | ImGuiWindowFlags_NoResize
                                   | ImGuiWindowFlags_NoCollapse
                                   | ImGuiWindowFlags_NoMove
                                   | ImGuiWindowFlags_NoSavedSettings
                                   | ImGuiWindowFlags_AlwaysAutoResize;

    // the menu has no close button: End() is paired unconditionally
    if (ImGui::Begin("##levelEditorToolMenu", nullptr, flags))
    {
        ImGui::TextUnformatted("Tools");

        // mode entries share the look of the sidebar buttons; selecting a
        // mode switches to it and closes the menu
        auto modeButton = [this](const char* label, Tool tool)
        {
            const bool active = mActiveTool == tool;
            if (active)
            {
                ImGui::PushStyleColor(ImGuiCol_Button,
                                      ImVec4(1.f, 0.55f, 0.f, 0.85f));
                ImGui::PushStyleColor(ImGuiCol_Text,
                                      ImVec4(0.f, 0.f, 0.f, 1.f));
            }
            if (ImGui::Button(label, ImVec2(-FLT_MIN, 0.f)))
            {
                switchTool(tool);
                mShowToolContextMenu = false;
            }
            if (active) { ImGui::PopStyleColor(2); }
        };

        modeButton("Resize", Tool::Resize);
        modeButton("Rotate", Tool::Rotate);
        ImGui::Separator();
        modeButton("Tile paint (LMB place / RMB erase)", Tool::TilePlacer);
        modeButton("Tile erase", Tool::TileRemover);
        ImGui::Separator();
        modeButton("Deactivate tool", Tool::None);
        ImGui::Separator();

        ImGui::BeginDisabled(mUndoStack.empty());
        if (ImGui::Button("Undo (STRG+Z)", ImVec2(-FLT_MIN, 0.f)))
        {
            undo();
            mShowToolContextMenu = false;
        }
        ImGui::EndDisabled();
        ImGui::BeginDisabled(mRedoStack.empty());
        if (ImGui::Button("Redo (STRG+Y)", ImVec2(-FLT_MIN, 0.f)))
        {
            redo();
            mShowToolContextMenu = false;
        }
        ImGui::EndDisabled();
    }

    // remember where the menu lives so clicks off the menu close it while
    // clicks on it keep working
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    mContextMenuLeft = pos.x;
    mContextMenuTop = pos.y;
    mContextMenuWidth = size.x;
    mContextMenuHeight = size.y;
    ImGui::End();
}

void
Editor::switchTool(Tool tool)
{
    if (mActiveTool == tool) { return; }
    mActiveTool = tool;
    if (tool == Tool::Resize || tool == Tool::Rotate)
    {
        if (mWorld == nullptr) { return; }
        // entering a transform mode: the object currently chosen in the
        // object list is selected in the level right away (outline + markers)
        mWorld->setSelectionStyle(tool == Tool::Rotate
            ? Renderer::SelectionStyle::RotateMarkers
            : Renderer::SelectionStyle::ResizeHandles);
        if (mSelectedIndex >= 0)
        {
            auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
            if (mSelectedIndex < static_cast<int>(objects.size()))
            {
                mSelectedBodyId = objects.at(mSelectedIndex).mPhysicsBodyId;
                mPivotOffsetPx = {};
                mWorld->setSelectedHighlight(mSelectedBodyId);
            }
        }
        updatePivotMarker();
    }
    else if (tileToolActive())
    {
        // tiles are painted free-standing: no persistent object selection
        exitSelectionMode();
        mShowTileBrushPreview = false;
        if (mWorld == nullptr) { return; }
        mWorld->setResizePreview({}, {}, false);
        syncTileGridToBrush();
    }
    else
    {
        // leaving the transform mode: clear the selection highlights
        exitSelectionMode();
    }
}

void
Editor::handleMouseEvent(const sf::Event &event, const sf::Vector2f &mouseWorldPosPx)
{
    if (mWorld == nullptr) { return; }

    // clicks/motion over the editor widgets stay with ImGui; a click on any
    // ImGui window except an open tools context menu closes that menu
    if (ImGui::GetIO().WantCaptureMouse)
    {
        if (mShowToolContextMenu
            && event.is<sf::Event::MouseButtonPressed>())
        {
            const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>();
            const sf::Vector2f pos{ pressed->position };
            const bool onMenu
                = pos.x >= mContextMenuLeft
                  && pos.x <= mContextMenuLeft + mContextMenuWidth
                  && pos.y >= mContextMenuTop
                  && pos.y <= mContextMenuTop + mContextMenuHeight;
            if (not onMenu) { mShowToolContextMenu = false; }
        }
        mWorld->setHoverHighlight(b2_nullBodyId);
        return;
    }

    // every click in the game view closes an open tools context menu
    // (a right click reopens it at the new cursor position)
    if (event.is<sf::Event::MouseButtonPressed>())
    {
        mShowToolContextMenu = false;
    }

    // tile tools: LMB paints/or erases, RMB erases (no context menu, no
    // object selection while a tile tool is active)
    if (tileToolActive())
    {
        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button == sf::Mouse::Button::Left)
            {
                beginTileStroke(false, mouseWorldPosPx);
                return;
            }
            if (pressed->button == sf::Mouse::Button::Right)
            {
                beginTileStroke(true, mouseWorldPosPx);
                return;
            }
        }
        if (const auto* released = event.getIf<sf::Event::MouseButtonReleased>())
        {
            if (released->button == sf::Mouse::Button::Left
                or released->button == sf::Mouse::Button::Right)
            {
                endTileStroke();
                return;
            }
        }
        if (event.is<sf::Event::MouseMoved>())
        {
            if (mTileStrokeActive)
            {
                updateTileStroke(mouseWorldPosPx);
            }
            else
            {
                // idle brush preview: orange overlay on the hovered cell
                const sf::Vector2i cell = tileCellAt(mouseWorldPosPx);
                mWorld->setResizePreview(
                    tileCellCenterPx(cell),
                    { static_cast<float>(mGridCellWidth),
                      static_cast<float>(mGridCellHeight) },
                    true, 0.f);
            }
            return;
        }
        return;
    }

    // right click: context menu with the editor tools at the cursor
    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button == sf::Mouse::Button::Right)
        {
            openToolContextMenu(pressed->position);
            return;
        }
    }

    if (mActiveTool != Tool::Resize && mActiveTool != Tool::Rotate)
    {
        // no transform tool: hover highlights the object, a left click
        // selects it (orange fill, no handles); no dragging here
        if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
        {
            if (pressed->button == sf::Mouse::Button::Left)
            {
                selectObjectAt(mouseWorldPosPx);
            }
            return;
        }
        if (event.is<sf::Event::MouseMoved>() && mDragMode == DragMode::None)
        {
            const ELevel activeLevel = mWorld->getActiveLevel();
            // getSceneObjectAt hit-tests the object rects in PIXELS
            const b2Vec2 positionPx{ mouseWorldPosPx.x, mouseWorldPosPx.y };
            mWorld->setHoverHighlight(
                mWorld->getSceneObjectAt(activeLevel, positionPx));
        }
        return;
    }

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button != sf::Mouse::Button::Left) { return; }

        if (mMainCharRowSelected)
        {
            // the character body is animation-driven: no rotation support;
            // in rotate mode the press is simply ignored
            if (mActiveTool == Tool::Rotate) { return; }
            // press inside the character overlay: grab + move it like an
            // object; press elsewhere: deselect the character
            const Physics::CharacterPhysicsRect rect = mWorld->getCharacterRect();
            const sf::Vector2f centerPx{
                Converter::metersToPixels(rect.centerM.x),
                Converter::metersToPixels(rect.centerM.y)};
            const sf::Vector2f halfSize = rect.sizePx / 2.f;
            if (std::abs(mouseWorldPosPx.x - centerPx.x) <= halfSize.x
                && std::abs(mouseWorldPosPx.y - centerPx.y) <= halfSize.y)
            {
                beginDrag(DragMode::Move, -1, mouseWorldPosPx);
                return;
            }
            mMainCharRowSelected = false;
            mWorld->setCharacterSelection({}, {}, false);
            syncEditBuffersFromSelection();
            return;
        }

        if (not B2_IS_NULL(mSelectedBodyId))
        {
            if (mActiveTool == Tool::Rotate)
            {
                // press on the pivot marker: move the pivot inside the object
                if (pivotMarkerAt(mouseWorldPosPx))
                {
                    beginDrag(DragMode::MovePivot, -1, mouseWorldPosPx);
                    return;
                }
                // press on a corner marker: rotate around the pivot
                const int marker = rotateMarkerAt(mouseWorldPosPx);
                if (marker >= 0)
                {
                    beginDrag(DragMode::Rotate, marker, mouseWorldPosPx);
                    return;
                }
                // press inside the object keeps the selection (moving is a
                // resize-mode feature); press on empty game view deselects
                if (insideSelectedObject(mouseWorldPosPx)) { return; }
                mSelectedBodyId = b2_nullBodyId;
                mWorld->setSelectedHighlight(b2_nullBodyId);
                updatePivotMarker();
                syncEditBuffersFromSelection();
                return;
            }

            const int handle = resizeHandleAt(mouseWorldPosPx);
            if (handle >= 0)
            {
                beginDrag(DragMode::Resize, handle, mouseWorldPosPx);
                return;
            }
            if (insideSelectedObject(mouseWorldPosPx))
            {
                beginDrag(DragMode::Move, -1, mouseWorldPosPx);
                return;
            }
            // click on empty game view: deselect the object (the tool
            // itself stays on)
            mSelectedBodyId = b2_nullBodyId;
            mWorld->setSelectedHighlight(b2_nullBodyId);
            syncEditBuffersFromSelection();
            return;
        }

        // no object selected yet: select the one under the cursor
        const ELevel activeLevel = mWorld->getActiveLevel();
        // getSceneObjectAt hit-tests the object rects in PIXELS
        const b2Vec2 positionPx{ mouseWorldPosPx.x, mouseWorldPosPx.y };
        const b2BodyId hit = mWorld->getSceneObjectAt(activeLevel, positionPx);
        if (B2_IS_NULL(hit)) { return; }

        mSelectedBodyId = hit;
        // keep the object list in sync with the viewport selection
        mMainCharRowSelected = false;
        mWorld->setCharacterSelection({}, {}, false);
        auto& objects = mWorld->getSceneObjects().at(activeLevel);
        for (int i = 0; i < static_cast<int>(objects.size()); ++i)
        {
            if (bodyIdsEqual(objects.at(i).mPhysicsBodyId, hit))
            {
                mSelectedIndex = i;
                break;
            }
        }
        mLastSelectedIndex = -1;
        mSelectedBodyId = hit;
        mPivotOffsetPx = {};
        mWorld->setSelectedHighlight(hit);
        updatePivotMarker();
        syncEditBuffersFromSelection();
        return;
    }

    if (event.is<sf::Event::MouseButtonReleased>()
        && event.getIf<sf::Event::MouseButtonReleased>()->button
             == sf::Mouse::Button::Left)
    {
        if (mDragMode != DragMode::None) { endDrag(); }
        return;
    }

    if (event.is<sf::Event::MouseMoved>())
    {
        if (mDragMode != DragMode::None)
        {
            updateDrag(mouseWorldPosPx);
            return;
        }
        // hover highlight (only outside the selection mode); while the
        // main-character pseudo-entry is selected the overlay stays clean
        if (B2_IS_NULL(mSelectedBodyId) && not mMainCharRowSelected)
        {
            const ELevel activeLevel = mWorld->getActiveLevel();
            // getSceneObjectAt hit-tests the object rects in PIXELS
            const b2Vec2 positionPx{ mouseWorldPosPx.x, mouseWorldPosPx.y };
            const b2BodyId hit = mWorld->getSceneObjectAt(activeLevel,
                                                          positionPx);
            mWorld->setHoverHighlight(hit);
        }
        else
        {
            // keep the selection clean of hover states while it is active
            mWorld->setHoverHighlight(b2_nullBodyId);
        }
    }
}

void
Editor::drawSelectionModeHud()
{
    // the text belongs purely to the active tool state, not to an object
    if (mActiveTool != Tool::Resize && mActiveTool != Tool::Rotate
        && not tileToolActive()) { return; }

    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    const ImVec2 pos{ viewport->WorkSize.x - 240.f, viewport->WorkSize.y - 36.f };
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, {0.f, 0.f});
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                             | ImGuiWindowFlags_NoBackground
                             | ImGuiWindowFlags_NoMove
                             | ImGuiWindowFlags_NoBringToFrontOnFocus
                             | ImGuiWindowFlags_NoNav;
    ImGui::Begin("##selectionModeHud", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_Text, {1.f, 0.f, 0.f, 1.f});
    const char* modeText = "tool active";
    if (mActiveTool == Tool::Rotate) { modeText = "rotate-mode active"; }
    else if (mActiveTool == Tool::Resize) { modeText = "resize-mode active"; }
    else if (mActiveTool == Tool::TilePlacer)
    {
        modeText = "tile-placer active (LMB place, RMB erase)";
    }
    else if (mActiveTool == Tool::TileRemover)
    {
        modeText = "tile-remover active (LMB/RMB erase)";
    }
    ImGui::Text("%s", modeText);
    ImGui::PopStyleColor();
    ImGui::End();
}

void
Editor::drawObjectDetails(SceneObjectPhysicsParameters *object)
{
    // Enter in any edit field applies all pending edits at once
    // (like clicking 'Apply edits')
    bool applyOnEnter = false;
    const ImGuiInputTextFlags enterFlags = ImGuiInputTextFlags_EnterReturnsTrue;

    // ImGui core InputText needs a char*; keep the std::string as the model
    char nameBuf[64]{};
    std::copy_n(mEditName.c_str(), sizeof(nameBuf) - 1, nameBuf);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf), enterFlags))
    {
        applyOnEnter = true;
    }
    if (ImGui::IsItemEdited()) { mEditName = nameBuf; }
    int typeItem = mEditIsDynamic ? 1 : 0;
    if (ImGui::Combo("BodyType", &typeItem, kBodyTypeNames, kNumBodyTypes))
    {
        mEditIsDynamic = typeItem == 1;
    }
    if (ImGui::Checkbox("Physics (collides)", &mEditHasPhysics))
    {
        applyEdits();
    }
    if (not mEditHasPhysics)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(decoration)");
    }
    ImGui::Combo("EntityType", &mEditEntityType, kEntityTypeNames, kNumEntityTypes);
    // texture: live preview + button that opens the registry picker window
    // (whole images and tilesets with per-tile selection)
    drawTexturePreview({ 48.f, 48.f });
    ImGui::SameLine();
    ImGui::BeginGroup();
    const std::string tileLabel =
        mEditTileCol >= 0
            ? fmt::format("tile [{},{}]", mEditTileCol, mEditTileRow)
            : std::string("whole image");
    if (ImGui::Button(fmt::format("Pick texture: {}\n({})",
                                  mEditTextureId, tileLabel).c_str(),
                      { 170.f, 0.f }))
    {
        mShowTexturePicker = not mShowTexturePicker;
    }
    ImGui::TextDisabled("drop pngs into assets/scenes/objects");
    ImGui::EndGroup();
    ImGui::Checkbox("Paint over tiles (replace)", &mReplaceTiles);
    if (mReplaceTiles)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(big geometry stays)");
    }
    if (ImGui::InputInt("Layer (0=BG,1=G1,2=FG)", &mEditLayer, 1, 100,
                        enterFlags))
    {
        applyOnEnter = true;
    }
    if (ImGui::DragFloat("PosX", &mEditPositionX, 0.5f, 0.f, 640.f,
                         "%.3f", enterFlags))
    {
        applyOnEnter = true;
    }
    if (ImGui::DragFloat("PosY", &mEditPositionY, 0.5f, 0.f, 360.f,
                         "%.3f", enterFlags))
    {
        applyOnEnter = true;
    }
    if (ImGui::DragFloat("Angle", &mEditAngle, 1.f, -360.f, 360.f,
                         "%.3f", enterFlags))
    {
        applyOnEnter = true;
    }
    if (ImGui::DragFloat("Width", &mEditWidth, 0.5f, 1.f, 640.f,
                         "%.3f", enterFlags))
    {
        applyOnEnter = true;
    }
    if (ImGui::DragFloat("Height", &mEditHeight, 0.5f, 1.f, 360.f,
                         "%.3f", enterFlags))
    {
        applyOnEnter = true;
    }

    if (applyOnEnter && object != nullptr)
    {
        mEditName = nameBuf;
        applyEdits();
    }

    if (ImGui::Button("Apply edits") && object != nullptr)
    {
        applyEdits();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add object")) { addObject(); }

    if (mShowTexturePicker) { drawTexturePicker(); }
}

void
Editor::drawTexturePreview(const sf::Vector2f& boxSize)
{
    if (not mWorld)
    {
        ImGui::Dummy({ boxSize.x, boxSize.y });
        return;
    }
    auto& registry = mWorld->getTextureRegistry();
    const sf::Texture* texture = registry.texture(mEditTextureId);
    const sf::Texture& shown =
        texture != nullptr ? *texture : registry.fallbackTexture();
    const sf::IntRect rect = registry.textureRect(mEditTextureId,
                                                  mEditTileCol, mEditTileRow);
    sf::Sprite sprite(shown, rect);
    // fit into the preview box, keeping the aspect ratio
    const float rectW = std::max(1.f, static_cast<float>(rect.size.x));
    const float rectH = std::max(1.f, static_cast<float>(rect.size.y));
    const float fit = std::min(boxSize.x / rectW, boxSize.y / rectH);
    const sf::Vector2f display{ rectW * fit, rectH * fit };
    ImGui::Image(sprite, display);
    textureSizeTooltip(registry, mEditTextureId, mEditTileCol, mEditTileRow);
    if (texture == nullptr)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("not found");
    }
}

void
Editor::drawTexturePicker()
{
    if (not mWorld) { mShowTexturePicker = false; return; }
    auto& registry = mWorld->getTextureRegistry();

    ImGui::SetNextWindowSize({ 420.f, 400.f }, ImGuiCond_Appearing);
    if (not ImGui::Begin("Texture picker", &mShowTexturePicker))
    {
        ImGui::End();
        return;
    }

    // hovered tile of a tileset, magnified at the bottom of the window
    bool hasHover = false;
    std::optional<sf::Sprite> hoverTile;
    std::string hoverName;
    std::string hoverId;
    int hoverCol = -1;
    int hoverRow = -1;

    const float cell = 40.f;
    if (ImGui::CollapsingHeader("Whole images", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const auto& id : registry.imageIds())
        {
            const sf::Texture* texture = registry.texture(id);
            const sf::Texture& shown =
                texture != nullptr ? *texture : registry.fallbackTexture();
            sf::Sprite sprite(shown,
                              registry.textureRect(id, -1, -1));
            ImGui::PushID(id.c_str());
            if (ImGui::ImageButton("whole_image", sprite, { cell, cell }))
            {
                mEditTextureId = id;
                mEditTileCol = -1;
                mEditTileRow = -1;
                mShowTexturePicker = false;
                applyEdits();
            }
            ImGui::PopID();
            textureSizeTooltip(registry, id, -1, -1);
            if (ImGui::IsItemHovered())
            {
                hasHover = true;
                hoverTile = sprite;
                hoverName = id;
                hoverId = id;
                hoverCol = -1;
                hoverRow = -1;
            }
            ImGui::SameLine();
            ImGui::TextUnformatted(id.c_str());
            if (mEditTextureId == id and mEditTileCol < 0)
            {
                ImGui::SameLine();
                ImGui::TextColored({ 1.f, 0.6f, 0.f, 1.f }, "(current)");
            }
        }
        ImGui::NewLine();
    }

    if (ImGui::CollapsingHeader("Tilesets", ImGuiTreeNodeFlags_DefaultOpen))
    {
        for (const auto& id : registry.tilesetIds())
        {
            const auto* info = registry.tileset(id);
            const sf::Texture* texture = registry.texture(id);
            if (info == nullptr || texture == nullptr
                || info->columns <= 0 || info->rows <= 0)
            {
                continue;
            }
            if (not ImGui::TreeNode(id.c_str()))
            {
                continue;
            }
            for (int row = 0; row < info->rows; ++row)
            {
                for (int col = 0; col < info->columns; ++col)
                {
                    sf::Sprite tile(*texture,
                                    registry.textureRect(id, col, row));
                    ImGui::PushID(row * info->columns + col);
                    if (ImGui::ImageButton("tile", tile, { cell, cell }))
                    {
                        mEditTextureId = id;
                        mEditTileCol = col;
                        mEditTileRow = row;
                        mShowTexturePicker = false;
                        applyEdits();
                    }
                    ImGui::PopID();
                    textureSizeTooltip(registry, id, col, row);
                    if (ImGui::IsItemHovered())
                    {
                        hasHover = true;
                        hoverTile = tile;
                        hoverName =
                            fmt::format("{} [{},{}]", id, col, row);
                        hoverId = id;
                        hoverCol = col;
                        hoverRow = row;
                    }
                    const bool currentTile = mEditTextureId == id
                        && mEditTileCol == col && mEditTileRow == row;
                    if (currentTile)
                    {
                        ImGui::GetWindowDrawList()->AddRect(
                            ImGui::GetItemRectMin(), ImGui::GetItemRectMax(),
                            IM_COL32(255, 160, 0, 255), 2.f);
                    }
                    if (col < info->columns - 1)
                    {
                        ImGui::SameLine();
                    }
                }
            }
            ImGui::TreePop();
        }
    }

    // magnified hover preview (whole image or hovered tile)
    ImGui::Separator();
    if (not hasHover)
    {
        const sf::Texture* texture = registry.texture(mEditTextureId);
        const sf::Texture& shown =
            texture != nullptr ? *texture : registry.fallbackTexture();
        hoverTile = sf::Sprite(shown, registry.textureRect(mEditTextureId,
                                                          mEditTileCol,
                                                          mEditTileRow));
        hoverName = fmt::format("{} ({})", mEditTextureId,
                                mEditTileCol >= 0 ? "tile" : "whole image");
    }
    ImGui::TextUnformatted(hoverName.c_str());
    ImGui::Image(*hoverTile, { 96.f, 96.f });
    textureSizeTooltip(registry, hasHover ? hoverId : mEditTextureId,
                       hasHover ? hoverCol : mEditTileCol,
                       hasHover ? hoverRow : mEditTileRow);

    ImGui::End();
}

void
Editor::createEditorWidgets(bool *p_open)
{
    if (*p_open == false)
    {
        // the editor is closed: never leave a dangling selection/tool/hover
        // behind; the level must return to its original playing state
        mActiveTool = Tool::None;
        exitSelectionMode();
        mMainCharRowSelected = false;
        if (mWorld != nullptr)
        {
            mWorld->setCharacterSelection({}, {}, false);
        }
        if (mShowGrid && mWorld != nullptr)
        {
            mShowGrid = false;
            mWorld->setGridOverlay(false, {32.f, 32.f}, sf::Color::Red);
        }
        mParallaxSyncNeeded = true;
        if (mCameraPreviewOffsetPx != sf::Vector2f{} && mWorld != nullptr)
        {
            // restore the fixed-canvas camera when the editor closes
            mCameraPreviewOffsetPx = {};
            mWorld->setWorldViewCenterPx({ 320.f, 180.f });
        }
        mRenameActive = false;
        mShowEditorSettings = false;
        mShowToolContextMenu = false;
        return;
    }

    drawToolSidebar();
    if (mParallaxSyncNeeded && mWorld != nullptr)
    {
        for (std::size_t i = 0; i < 3; ++i)
        {
            mParallax[i] = mWorld->getLayerParallax(i);
        }
        mParallaxSyncNeeded = false;
    }
    drawGridControls();
    drawParallaxControls();

    if (ImGui::Begin("Ryu Level Editor", p_open))
    {
        if (mWorld == nullptr)
        {
            ImGui::Text("No world attached to the editor.");
            ImGui::End();
            return;
        }

        // Level switching: every level slot, labeled with its custom name
        // when the level file carries one; missing (empty) slots exist only
        // for levels created with the '+ create level' button
        std::vector<std::string> levelLabels;
        for (auto level : ELevel::_values())
        {
            std::string label = level._to_string();
            if (const auto name = mWorld->getLevelNames().find(level);
                name != mWorld->getLevelNames().end()
                and not name->second.empty())
            {
                label = fmt::format("{} ({})", name->second,
                                    level._to_string());
            }
            levelLabels.push_back(std::move(label));
        }
        std::vector<const char*> levelItems;
        levelItems.reserve(levelLabels.size());
        for (const auto& label : levelLabels)
        {
            levelItems.push_back(label.c_str());
        }
        int levelItem = mSelectedLevel._to_integral();
        if (ImGui::Combo("Level", &levelItem, levelItems.data(),
                         static_cast<int>(levelItems.size())))
        {
            if (ELevel::_is_valid(levelItem))
            {
                const auto chosen = ELevel::_from_integral(levelItem);
                // only levels with data can be opened: empty slots are
                // filled through the create-level window
                if (chosen != mSelectedLevel
                    and mWorld->getSceneObjects().contains(chosen))
                {
                    switchLevel(chosen);
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("+ create level")) { mShowCreateLevel = true; }

        // Object list
        auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
        // hovered/selected rows fill in a translucent orange
        ImGui::PushStyleColor(ImGuiCol_Header,
                              ImVec4(1.f, 0.55f, 0.f, 0.45f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                              ImVec4(1.f, 0.55f, 0.f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                              ImVec4(1.f, 0.55f, 0.f, 0.85f));
        if (ImGui::BeginChild("Objects", ImVec2(0, 200), true))
        {
            // MAIN CHARACTER pseudo-entry: selecting it moves the character
            // via the usual positioning/drag mechanisms
            if (ImGui::Selectable("MainCharacter [dynamic]",
                                  mMainCharRowSelected))
            {
                mMainCharRowSelected = true;
                mSelectedIndex = -1;
                mLastSelectedIndex = -1;
                mSelectedBodyId = b2_nullBodyId;
                mWorld->setSelectedHighlight(b2_nullBodyId);
                syncCharEditBuffers();
                showMainCharSelection(mWorld);
            }
            for (int i = 0; i < static_cast<int>(objects.size()); ++i)
            {
                const auto& object = objects.at(i);
                const std::string label = fmt::format(
                    "{}. {} [{}]", i, object.mName,
                    object.mType == b2_dynamicBody ? "dynamic" : "static");

                if (ImGui::Selectable(label.c_str(), mSelectedIndex == i))
                {
                    mSelectedIndex = i;
                    mMainCharRowSelected = false;
                    mWorld->setCharacterSelection({}, {}, false);
                    mLastSelectedIndex = -1;
                    syncEditBuffersFromSelection();

                    // in a transform mode a list click selects the object
                    // in the level (persistent outline + markers); without
                    // such a tool the list click only fills the edit buffers
                    if (transformToolActive())
                    {
                        mSelectedBodyId = object.mPhysicsBodyId;
                        mPivotOffsetPx = {};
                        mWorld->setSelectedHighlight(mSelectedBodyId);
                        mWorld->setSelectionStyle(mActiveTool == Tool::Rotate
                            ? Renderer::SelectionStyle::RotateMarkers
                            : Renderer::SelectionStyle::ResizeHandles);
                        updatePivotMarker();
                    }
                }

                // double-click on a row renames the object (Enter/Apply
                // in the rename window commits the new name)
                if (ImGui::IsItemHovered()
                    && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                {
                    beginRename(i);
                }

                // IsItemHovered() refers to the LAST submitted item, so it
                // must be queried AFTER Selectable (otherwise the hover
                // highlight lands on the previous row)
                // in selection mode the hovered object also gets outline +
                // edge markers, otherwise only the orange fill
                if (ImGui::IsItemHovered())
                {
                    mHoveredBodyId = object.mPhysicsBodyId;
                    mWorld->setHoverHighlight(mHoveredBodyId,
                                              transformToolActive());
                }
            }

            // transient hover marking resets as soon as the mouse leaves the
            // list (only an explicit left-click keeps an object selected)
            if (not ImGui::IsAnyItemHovered() && not B2_IS_NULL(mHoveredBodyId))
            {
                mHoveredBodyId = b2_nullBodyId;
                mWorld->setHoverHighlight(b2_nullBodyId, false);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(3);

        // Details of the selected object; the edit buffers are only synced
        // when the selection changes, otherwise the widgets would clobber
        // the user's in-progress edits every frame.
        SceneObjectPhysicsParameters* selected = nullptr;
        if (mSelectedIndex >= 0 && mSelectedIndex < static_cast<int>(objects.size()))
        {
            selected = &objects.at(mSelectedIndex);
            if (mSelectedIndex != mLastSelectedIndex)
            {
                syncEditBuffersFromSelection();
                mLastSelectedIndex = mSelectedIndex;
            }
        }
        drawObjectDetails(selected);

        if (ImGui::Button("Delete object")) { deleteObject(); }
        ImGui::SameLine();
        if (ImGui::Button("Reload level")) { switchLevel(mSelectedLevel); }
        ImGui::SameLine();
        if (ImGui::Button("Save level")) { saveLevel(); }
        ImGui::SameLine();
        if (ImGui::Button("Editor settings")) { mShowEditorSettings = true; }

        ImGui::Separator();
        ImGui::TextWrapped(
            "Edit the fields and hit 'Apply edits' - it recreates the "
            "physics body live. 'Save level' writes assets/levels/levelN.json "
            "(relative to the working directory).");
    }
    ImGui::End();

    drawSelectionModeHud();
    drawRenameWindow();
    drawCreateLevelWindow();
    drawEditorSettings();
    drawToolContextMenu();
}

} // namespace RyuLevel
