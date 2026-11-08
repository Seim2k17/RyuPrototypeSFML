#include <RyuSuite/LevelEditor.h>

#include <Ryu/Core/World.h>
#include <Ryu/Physics/Physics.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Scene/LevelData.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Core/Renderer.h>

#include <imgui.h>
#include <fmt/core.h>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

#include <array>
#include <string>

namespace RyuLevel
{

namespace
{

constexpr int kNumBodyTypes = 2;
const char *kBodyTypeNames[kNumBodyTypes] = { "Static", "Dynamic" };

constexpr int kNumTextures = 10;
const char *kTextureNames[kNumTextures] = {
    "BGMountain", "BoxPushable", "BoxHeavy", "Button", "Grass",
    "Grate", "Ground", "Ladder", "Teleport", "Unknown"
};

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

Textures::SceneID
textureIdFromIndex(int index)
{
    switch (index)
    {
    case 0: return Textures::SceneID::BGMountain;
    case 1: return Textures::SceneID::BoxPushable;
    case 2: return Textures::SceneID::BoxHeavy;
    case 3: return Textures::SceneID::Button;
    case 4: return Textures::SceneID::Grass;
    case 5: return Textures::SceneID::Grate;
    case 6: return Textures::SceneID::Ground;
    case 7: return Textures::SceneID::Ladder;
    case 8: return Textures::SceneID::Teleport;
    default: return Textures::SceneID::Unknown;
    }
}

int
textureIdToIndex(Textures::SceneID id)
{
    for (int i = 0; i < kNumTextures; ++i)
    {
        if (textureIdFromIndex(i) == id) { return i; }
    }
    return kNumTextures - 1;
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
    , mEditTextureId(textureIdToIndex(Textures::SceneID::Grass))
    , mEditEntityType(0)
    , mEditLayer(1)
    , mEditPositionX(320.f)
    , mEditPositionY(180.f)
    , mEditWidth(64.f)
    , mEditHeight(16.f)
    , mDragMode(DragMode::None)
    , mDragHandle(-1)
    , mDragStartCenterPx({})
    , mDragStartSizePx({})
    , mDragHandleStartPx({})
    , mLastPreviewCenterPx({})
    , mLastPreviewSizePx({})
    , mActiveTool(Tool::None)
    , mSelectedBodyId(b2_nullBodyId)
    , mHoveredBodyId(b2_nullBodyId)
    , mShowGrid(false)
    , mGridCellWidth(32)
    , mGridCellHeight(32)
    , mGridColorIndex(0)
    , mMainCharRowSelected(false)
{
}

void
Editor::switchLevel(ELevel level)
{
    exitSelectionMode();
    mSelectedLevel = level;
    mSelectedIndex = -1;
    mMainCharRowSelected = false;
    if (mWorld)
    {
        mWorld->setCharacterSelection({}, {}, false);
        mWorld->reloadSceneObjects(level);
        syncEditBuffersFromSelection();
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
        textureIdFromIndex(mEditTextureId),
        mEditEntityType == 1 ? EntityType::Climbable : EntityType::None,
        mEditLayer
    };
    auto& added = mWorld->addSceneObject(mSelectedLevel, std::move(object));
    mSelectedIndex = static_cast<int>(
        mWorld->getSceneObjects().at(mSelectedLevel).size()) - 1;
    mLastSelectedIndex = -1; // refresh edit buffers from the new object
    syncEditBuffersFromSelection();
    fmt::print("LevelEditor: added object '{}'.\n", added.mName);
}

void
Editor::deleteObject()
{
    if (not mWorld || mSelectedIndex < 0) { return; }
    auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
    if (mSelectedIndex >= static_cast<int>(objects.size())) { return; }
    exitSelectionMode();
    mWorld->removeSceneObject(objects.at(mSelectedIndex).mPhysicsBodyId);
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
    object.mName = mEditName;
    object.mType = mEditIsDynamic ? b2_dynamicBody : b2_staticBody;
    object.mTextureId = textureIdFromIndex(mEditTextureId);
    object.mEntityType = mEditEntityType == 1 ? EntityType::Climbable
                                              : EntityType::None;
    object.mLayer = mEditLayer;
    object.mPosition = { Converter::pixelsToMeters(mEditPositionX),
                         Converter::pixelsToMeters(mEditPositionY) };
    object.mSize = { Converter::pixelsToMeters(mEditWidth),
                     Converter::pixelsToMeters(mEditHeight) };

    // recreate and keep the in-level selection highlight on the new body
    mSelectedBodyId = mWorld->updateSceneObject(object.mPhysicsBodyId);
    mWorld->setSelectedHighlight(mSelectedBodyId);
}

void
Editor::saveLevel()
{
    if (not mWorld) { return; }
    RyuLevel::saveLevelData(mSelectedLevel,
                            mWorld->getSceneObjects().at(mSelectedLevel));
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

    int nearestHandle = -1;
    float bestDistanceSq = kHandleHitRadius * kHandleHitRadius;
    for (int i = 0; i < kNumHandles; ++i)
    {
        const sf::Vector2f handleCenterPx{
            centerPx.x + halfPx.x * kHandles[i].relativePos.x,
            centerPx.y + halfPx.y * kHandles[i].relativePos.y};
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
    return std::abs(mouseWorldPosPx.x - centerPx.x) <= halfPx.x
           && std::abs(mouseWorldPosPx.y - centerPx.y) <= halfPx.y;
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

    mDragMode = mode;
    mDragHandle = handle;
    if (mode == DragMode::Resize)
    {
        mDragHandleStartPx = {
            mDragStartCenterPx.x
              + kHandles[handle].relativePos.x * mDragStartSizePx.x / 2.f,
            mDragStartCenterPx.y
              + kHandles[handle].relativePos.y * mDragStartSizePx.y / 2.f };
        // commit fallback for a press+release without any mouse movement
        mLastPreviewCenterPx = mDragStartCenterPx;
        mLastPreviewSizePx = mDragStartSizePx;
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
        const sf::Vector2f newCenter = mouseWorldPosPx - grabOffset;
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
        const sf::Vector2f newCenter = mouseWorldPosPx - grabOffset;
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
        const sf::Vector2f delta = mouseWorldPosPx - mDragHandleStartPx;

        sf::Vector2f newSize = mDragStartSizePx;
        sf::Vector2f newCenter = mDragStartCenterPx;

        // commit helper: the mask>0 edge (right/bottom) grows directly, the
        // mask<0 edge (left/top) follows the cursor in the mirrored size
        // direction; minimum size enforced. In BOTH cases the center moves
        // by delta/2: the cursor-following edge travels by delta and the
        // anchored edge stays, so their midpoint always shifts by delta/2.
        auto applyAxis = [&](int mask, float deltaAxis, float &sizeAxis,
                             float &centerAxis, float startSizeAxis,
                             float startCenterAxis)
        {
            if (mask == 0) { return; }
            if (mask > 0)
            {
                deltaAxis = std::max(deltaAxis, kMinObjectSize - startSizeAxis);
            }
            if (mask < 0)
            {
                deltaAxis = std::min(deltaAxis, startSizeAxis - kMinObjectSize);
            }
            sizeAxis = startSizeAxis + static_cast<float>(mask) * deltaAxis;
            centerAxis = startCenterAxis + deltaAxis / 2.f;
        };

        applyAxis(maskX, delta.x, newSize.x, newCenter.x,
                  mDragStartSizePx.x, mDragStartCenterPx.x);
        applyAxis(maskY, delta.y, newSize.y, newCenter.y,
                  mDragStartSizePx.y, mDragStartCenterPx.y);

        mLastPreviewCenterPx = newCenter;
        mLastPreviewSizePx = newSize;
        // keep the edit fields in sync with the live drag
        mEditPositionX = newCenter.x;
        mEditPositionY = newCenter.y;
        mEditWidth = newSize.x;
        mEditHeight = newSize.y;
        // live preview overlay in the game view; the body is recreated with
        // the final size when the drag ends
        mWorld->setResizePreview(newCenter, newSize, true);
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

    // the drag is over: the preview overlay must go away, otherwise a stale
    // orange outline would stay at the last previewed geometry
    mWorld->setResizePreview({}, {}, false);

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
    mEditTextureId = textureIdToIndex(object.mTextureId);
    mEditEntityType = object.mEntityType == EntityType::Climbable ? 1 : 0;
    mEditLayer = object.mLayer;
    mEditPositionX = Converter::metersToPixels(object.mPosition.x);
    mEditPositionY = Converter::metersToPixels(object.mPosition.y);
    mEditWidth = Converter::metersToPixels(object.mSize.x);
    mEditHeight = Converter::metersToPixels(object.mSize.y);
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
Editor::handleKey(sf::Keyboard::Scancode scancode)
{
    if (not mWorld) { return; }

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
    mWorld->setSelectedHighlight(mSelectedBodyId);
    mWorld->setCharacterSelection({}, {}, false);
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

    toolButton("Select", Tool::Select);
    ImGui::Separator();
    ImGui::BeginDisabled();
    toolButton("Edit", Tool::Edit);
    ImGui::EndDisabled();
    ImGui::Separator();

    if (mActiveTool == Tool::Select)
    {
        ImGui::TextWrapped("Selection-mode: hover to highlight, click to "
                           "select, drag inside to move, handles to resize. "
                           "Click the tool again to turn it off.");
    }
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
    }
}


void
Editor::switchTool(Tool tool)
{
    if (mActiveTool == tool) { return; }
    mActiveTool = tool;
    if (tool == Tool::Select)
    {
        // entering the selection mode: the object currently chosen in the
        // object list is selected in the level right away (outline + markers)
        if (mSelectedIndex >= 0)
        {
            auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
            if (mSelectedIndex < static_cast<int>(objects.size()))
            {
                mSelectedBodyId = objects.at(mSelectedIndex).mPhysicsBodyId;
                mWorld->setSelectedHighlight(mSelectedBodyId);
            }
        }
    }
    else
    {
        // leaving the selection mode: clear the selection highlights
        exitSelectionMode();
    }
}

void
Editor::handleMouseEvent(const sf::Event &event, const sf::Vector2f &mouseWorldPosPx)
{
    if (mWorld == nullptr) { return; }

    // clicks/motion over the editor widget stay with ImGui
    if (ImGui::GetIO().WantCaptureMouse)
    {
        mWorld->setHoverHighlight(b2_nullBodyId);
        return;
    }

    // the Select tool gates all selection interactions
    if (mActiveTool != Tool::Select)
    {
        mWorld->setHoverHighlight(b2_nullBodyId);
        return;
    }

    if (const auto* pressed = event.getIf<sf::Event::MouseButtonPressed>())
    {
        if (pressed->button != sf::Mouse::Button::Left) { return; }

        if (mMainCharRowSelected)
        {
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
            // click on empty game view: deselect the object (the Select tool
            // itself stays on)
            mSelectedBodyId = b2_nullBodyId;
            mWorld->setSelectedHighlight(b2_nullBodyId);
            syncEditBuffersFromSelection();
            return;
        }

        // no object selected yet: select the one under the cursor
        const ELevel activeLevel = mWorld->getActiveLevel();
        b2Vec2 positionM{ Converter::pixelsToMeters<float>(mouseWorldPosPx.x),
                          Converter::pixelsToMeters<float>(mouseWorldPosPx.y) };
        const b2BodyId hit = mWorld->getSceneObjectAt(activeLevel, positionM);
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
        mWorld->setSelectedHighlight(hit);
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
            b2Vec2 positionM{ Converter::pixelsToMeters<float>(mouseWorldPosPx.x),
                              Converter::pixelsToMeters<float>(mouseWorldPosPx.y) };
            const b2BodyId hit = mWorld->getSceneObjectAt(activeLevel, positionM);
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
    // the text belongs purely to the Select tool state, not to an object
    if (mActiveTool != Tool::Select) { return; }

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
    ImGui::Text("selection-mode active");
    ImGui::PopStyleColor();
    ImGui::End();
}

void
Editor::drawObjectDetails(SceneObjectPhysicsParameters *object)
{
    // ImGui core InputText needs a char*; keep the std::string as the model
    char nameBuf[64]{};
    std::copy_n(mEditName.c_str(), sizeof(nameBuf) - 1, nameBuf);
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
    {
        mEditName = nameBuf;
    }
    int typeItem = mEditIsDynamic ? 1 : 0;
    if (ImGui::Combo("BodyType", &typeItem, kBodyTypeNames, kNumBodyTypes))
    {
        mEditIsDynamic = typeItem == 1;
    }
    ImGui::Combo("Texture", &mEditTextureId, kTextureNames, kNumTextures);
    ImGui::Combo("EntityType", &mEditEntityType, kEntityTypeNames, kNumEntityTypes);
    ImGui::InputInt("Layer (0=BG,1=G1,2=FG)", &mEditLayer);
    ImGui::DragFloat("PosX", &mEditPositionX, 0.5f, 0.f, 640.f);
    ImGui::DragFloat("PosY", &mEditPositionY, 0.5f, 0.f, 360.f);
    ImGui::DragFloat("Width", &mEditWidth, 0.5f, 1.f, 640.f);
    ImGui::DragFloat("Height", &mEditHeight, 0.5f, 1.f, 360.f);

    if (ImGui::Button("Apply edits") && object != nullptr)
    {
        applyEdits();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add object")) { addObject(); }
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
        return;
    }

    drawToolSidebar();
    drawGridControls();

    if (ImGui::Begin("Ryu Level Editor", p_open))
    {
        if (mWorld == nullptr)
        {
            ImGui::Text("No world attached to the editor.");
            ImGui::End();
            return;
        }

        // Level switching
        int levelItem = mSelectedLevel._to_integral();
        if (ImGui::Combo("Level", &levelItem, "None\0Level1\0Level2\0Level3\0"))
        {
            if (ELevel::_is_valid(levelItem) && ELevel::_from_integral(levelItem) != mSelectedLevel)
            {
                switchLevel(ELevel::_from_integral(levelItem));
            }
        }

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

                    // in selection mode a list click selects the object in
                    // the level (persistent outline + markers); without the
                    // Select tool the list click only fills the edit buffers
                    if (mActiveTool == Tool::Select)
                    {
                        mSelectedBodyId = object.mPhysicsBodyId;
                        mWorld->setSelectedHighlight(mSelectedBodyId);
                    }
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
                                              mActiveTool == Tool::Select);
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

        ImGui::Separator();
        ImGui::TextWrapped(
            "Edit the fields and hit 'Apply edits' - it recreates the "
            "physics body live. 'Save level' writes assets/levels/levelN.json "
            "(relative to the working directory).");
    }
    ImGui::End();

    drawSelectionModeHud();
}

} // namespace RyuLevel
