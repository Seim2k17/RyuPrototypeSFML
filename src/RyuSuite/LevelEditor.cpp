#include <RyuSuite/LevelEditor.h>

#include <Ryu/Core/World.h>
#include <Ryu/Physics/Physics.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Scene/LevelData.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Core/Renderer.h>

#include <imgui.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Mouse.hpp>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <cstring>
#include <deque>
#include <fstream>
#include <string>

namespace RyuLevel
{

namespace
{

namespace fs = std::filesystem;
using json = nlohmann::json;

// Helper functions for JSON serialization (mirroring those in LevelData.cpp)
std::string bodyTypeToString(b2BodyType type)
{
    switch (type)
    {
    case b2_dynamicBody: return "dynamic";
    case b2_kinematicBody: return "kinematic";
    default: return "static";
    }
}

std::string sceneIdToString(Textures::SceneID id)
{
    switch (id)
    {
    case Textures::SceneID::BGMountain: return "BGMountain";
    case Textures::SceneID::BoxPushable: return "BoxPushable";
    case Textures::SceneID::BoxHeavy: return "BoxHeavy";
    case Textures::SceneID::Button: return "Button";
    case Textures::SceneID::Grass: return "Grass";
    case Textures::SceneID::Grate: return "Grate";
    case Textures::SceneID::Ground: return "Ground";
    case Textures::SceneID::Ladder: return "Ladder";
    case Textures::SceneID::Teleport: return "Teleport";
    default: return "Unknown";
    }
}

std::string entityTypeToString(EntityType type)
{
    return type == EntityType::Climbable ? "Climbable" : "None";
}

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
    , mShowNewLevelDialog(false)
    , mLevelDropdownCache()
    , mLevelDropdownDirty(true)
    , mMaxUndoSteps(30)
    , mDragUndoSnapshot()
    , mDragUndoIndex(-1)
    , mDragUndoValid(false)
    , mRenameBuffer{}
    , mRenameIndex(-1)
    , mRenameActive(false)
    , mShowEditorSettings(false)
{
    std::snprintf(mSettingsPath, sizeof(mSettingsPath),
                  "%s", "assets/levelEditorSettings.json");
    loadEditorSettings();
}

void
Editor::setWorld(World* world)
{
    mWorld = world;
    // Sync selected level with the current active level from the game
    if (mWorld != nullptr)
    {
        mSelectedLevel = mWorld->getActiveLevel();
    }
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
    // deleting an object shifts the indices of the following rows, so the
    // recorded undo entries would restore the wrong objects
    mUndoStack.clear();
    mRedoStack.clear();
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
    const UndoEntry undoEntry{ mSelectedLevel, mSelectedIndex, object, object };
    object.mName = mEditName;
    object.mType = mEditIsDynamic ? b2_dynamicBody : b2_staticBody;
    object.mTextureId = textureIdFromIndex(mEditTextureId);
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
    
    // Check if this level has a custom name
    auto customNameIt = mCustomLevelNames.find(mSelectedLevel);
    fmt::print("LevelEditor: Saving level {}, custom name lookup result: {}\n", 
                mSelectedLevel._to_string(), 
                customNameIt != mCustomLevelNames.end() ? customNameIt->second : "NOT FOUND");
    
    if (customNameIt != mCustomLevelNames.end() && !customNameIt->second.empty())
    {
        // Save with custom filename
        const std::string filename = fmt::format("assets/levels/{}.json", customNameIt->second);
        fmt::print("LevelEditor: Saving to custom filename: {}\n", filename);
        
        // Build JSON with custom name and eLevel
        json levelData = {
            { "levelName", customNameIt->second },
            { "eLevel", mSelectedLevel._to_integral() },
            { "objects", json::array() }
        };
        
        const auto& objects = mWorld->getSceneObjects().at(mSelectedLevel);
        for (const auto& obj : objects)
        {
            json objJson;
            objJson["name"] = obj.mName;
            objJson["positionPx"] = { Converter::metersToPixels(obj.mPosition.x),
                                      Converter::metersToPixels(obj.mPosition.y) };
            objJson["sizePx"] = { Converter::metersToPixels(obj.mSize.x),
                                  Converter::metersToPixels(obj.mSize.y) };
            objJson["bodyType"] = bodyTypeToString(obj.mType);
            objJson["textureId"] = sceneIdToString(obj.mTextureId);
            objJson["entityType"] = entityTypeToString(obj.mEntityType);
            objJson["layer"] = obj.mLayer;
            objJson["rotationDeg"] = Converter::radToDeg(obj.mRotation);
            levelData["objects"].push_back(objJson);
        }
        
        std::ofstream file(filename);
        if (file.is_open())
        {
            file << levelData.dump(1);
            fmt::print("LevelEditor: saved level to {}.\n", filename);
            // Invalidate cache since we created a new file
            mLevelDropdownDirty = true;
        }
        else
        {
            fmt::print("LevelEditor: failed to save level to {}.\n", filename);
        }
    }
    else
    {
        // Save with standard naming
        RyuLevel::saveLevelData(mSelectedLevel,
                                mWorld->getSceneObjects().at(mSelectedLevel));
        // Invalidate cache since we updated an existing file
        mLevelDropdownDirty = true;
    }
}

void
Editor::createNewLevel()
{
    if (not mWorld || mNewLevelName[0] == '\0') { return; }

    // Create the new level name as a proper ELevel
    // We'll use LevelX where X is the next available number
    // For now, we'll find the next available level number
    std::string levelNameStr = mNewLevelName;
    
    // Find next available level number
    // We support Level1 through Level10
    int nextLevelNum = 1;
    const int maxLevelNum = 10;
    
    while (nextLevelNum <= maxLevelNum)
    {
        if (!ELevel::_is_valid(nextLevelNum))
        {
            nextLevelNum++;
            continue;
        }
        
        ELevel testLevel = ELevel::_from_integral(nextLevelNum);
        // Check if this level already has objects loaded
        if (mWorld->getSceneObjects().find(testLevel) == mWorld->getSceneObjects().end())
        {
            // This level doesn't have objects yet, use it
            break;
        }
        nextLevelNum++;
    }
    
    if (nextLevelNum > maxLevelNum)
    {
        fmt::print("LevelEditor: Cannot create new level, all 10 level slots are in use.\n");
        return;
    }
    
    // If the user provided a custom name, we'll use it to create the file
    // but we need to map it to an ELevel enum value
    ELevel newLevel = ELevel::_from_integral(nextLevelNum);
    
    // Create a new empty scene objects vector for this level
    RyuLevel::SceneObjects newObjects;
    
    // Add a floor object (static body)
    // Screen dimensions: 640x360 pixels
    // Use constructor which takes pixels and converts to meters
    SceneObjectPhysicsParameters floorObject(
        b2Vec2{ 320.f, 351.f },   // Position in pixels (middle bottom)
        b2Vec2{ 640.f, 9.f },     // Size in pixels (full width, thin floor)
        "floor",                // Name
        b2_staticBody,          // Static body type
        Textures::SceneID::Grass, // Texture
        EntityType::None,       // Entity type
        1                       // Layer
    );
    newObjects.push_back(floorObject);
    
    // Save the level to file
    // The filename will be based on the level enum (level1.json, level2.json, etc.)
    // But we want to use the user's name. We need to handle this carefully.
    // For now, we'll create the file with the next available number
    // and the user's name will be stored in the levelName field of the JSON
    
    // Create the directory if it doesn't exist
    fs::create_directories("assets/levels");
    
    // Build the JSON manually
    json levelData = {
        { "levelName", levelNameStr },
        { "eLevel", nextLevelNum },  // Store the ELevel value for loading
        { "objects", json::array() }
    };
    
    // Add floor to JSON (convert from meters back to pixels)
    json floorJson;
    floorJson["name"] = floorObject.mName;
    floorJson["positionPx"] = { Converter::metersToPixels(floorObject.mPosition.x),
                                 Converter::metersToPixels(floorObject.mPosition.y) };
    floorJson["sizePx"] = { Converter::metersToPixels(floorObject.mSize.x),
                             Converter::metersToPixels(floorObject.mSize.y) };
    floorJson["bodyType"] = bodyTypeToString(floorObject.mType);
    floorJson["textureId"] = sceneIdToString(floorObject.mTextureId);
    floorJson["entityType"] = entityTypeToString(floorObject.mEntityType);
    floorJson["layer"] = floorObject.mLayer;
    floorJson["rotationDeg"] = Converter::radToDeg(floorObject.mRotation);
    levelData["objects"].push_back(floorJson);
    
    // Create the filename with the custom name
    std::string safeName = levelNameStr;
    // Remove any path separators or invalid characters
    std::replace(safeName.begin(), safeName.end(), '/', '_');
    std::replace(safeName.begin(), safeName.end(), '\\', '_');
    std::replace(safeName.begin(), safeName.end(), ':', '_');
    
    std::string filename = fmt::format("assets/levels/{}.json", safeName);
    
    std::ofstream file(filename);
    if (!file.is_open())
    {
        fmt::print("LevelEditor: Failed to create file {}.\n", filename);
        return;
    }
    file << levelData.dump(1);
    file.close();
    
    fmt::print("LevelEditor: Created new level '{}' at {}.\n", levelNameStr, filename);
    
    // Invalidate the dropdown cache so it will be rebuilt with the new level
    mLevelDropdownDirty = true;
    
    // Reload custom names to ensure the dropdown is updated
    loadCustomLevelNames();
    
    // Add the new level to the scene objects map
    mWorld->getSceneObjects()[newLevel] = std::move(newObjects);
    
    // Switch to the new level
    switchLevel(newLevel);
    
    // Position the character in the middle of the screen (320, 180)
    // This is done after switching so the character is in the new level
    mWorld->moveMainCharacterToPx({320.f, 180.f});
    
    // Reset the dialog
    mShowNewLevelDialog = false;
    mNewLevelName[0] = '\0';
}

/// ---- Custom level name handling ---------------------------------

std::string
Editor::getLevelDisplayName(ELevel level) const
{
    auto it = mCustomLevelNames.find(level);
    if (it != mCustomLevelNames.end() && !it->second.empty())
    {
        return it->second;
    }
    // Default to "LevelN" if no custom name
    return level._to_string();
}

void
Editor::loadCustomLevelNames()
{
    namespace fs = std::filesystem;
    const std::string directory = "assets/levels";
    
    std::error_code ec;
    std::string actualDirectory = directory;
    
    if (not fs::exists(directory, ec)) {
        // Try alternative path (parent directory's assets)
        std::string altPath = "../assets/levels";
        if (fs::exists(altPath, ec)) {
            actualDirectory = altPath;
        } else {
            return;
        }
    }
    
    // Clear existing mappings
    mCustomLevelNames.clear();
    
    // Invalidate the dropdown cache since we're about to reload level names
    mLevelDropdownDirty = true;
    
    // Load custom names from all .json files
    const std::regex levelFileRe("^level(\\d+)\\.json$");
    
    for (const auto &entry : fs::directory_iterator{ actualDirectory })
    {
        const std::string filename = entry.path().filename().string();
        
        // Only process .json files
        if (entry.path().extension() != ".json") { continue; }
        
        try {
            json data;
            std::ifstream f(entry.path());
            f >> data;
            
            std::string levelName;
            int eLevel = -1;
            
            // Try to get levelName and eLevel from JSON
            if (data.contains("levelName"))
            {
                levelName = data["levelName"].get<std::string>();
            }
            if (data.contains("eLevel"))
            {
                eLevel = data["eLevel"].get<int>();
            }
            
            // If this is a standard levelN.json file
            std::smatch match;
            if (std::regex_match(filename, match, levelFileRe))
            {
                const auto levelId = std::stoi(match[1]);
                if (ELevel::_is_valid(levelId))
                {
                    ELevel level = ELevel::_from_integral(levelId);
                    // Use eLevel from JSON if available, otherwise use the filename
                    if (eLevel >= 1 && eLevel <= 10 && ELevel::_is_valid(eLevel))
                    {
                        level = ELevel::_from_integral(eLevel);
                    }
                    
                    // Store custom name if it's not just "1", "2", "3", etc.
                    if (!levelName.empty() && levelName != level._to_string())
                    {
                        mCustomLevelNames[level] = levelName;
                    }
                }
            }
            // For custom-named files, use the eLevel from JSON
            else if (eLevel >= 1 && eLevel <= 10 && ELevel::_is_valid(eLevel) && !levelName.empty())
            {
                ELevel level = ELevel::_from_integral(eLevel);
                // For custom files, always use the levelName if available
                mCustomLevelNames[level] = levelName;
            }
        }
        catch (const std::exception &e) {
            fmt::print("LevelEditor: failed to parse {} for custom name: {}.\n", filename, e.what());
        }
    }
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
                                mDragUndoSnapshot, after });
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
    mEditTextureId = textureIdToIndex(object.mTextureId);
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
    // ignored while ImGui owns the keyboard (e.g. typing in a text field)
    if (key.control && not ImGui::GetIO().WantCaptureKeyboard)
    {
        if (key.scancode == sf::Keyboard::Scancode::Z)
        {
            if (key.shift) { redo(); } else { undo(); }
            return;
        }
        if (key.scancode == sf::Keyboard::Scancode::Y)
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
        || lhs.mLayer != rhs.mLayer;
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
                UndoEntry entry{ mSelectedLevel, mRenameIndex, object, object };
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
                           "STRG+SHIFT+Z (or STRG+Y) redoes it. Up to "
                           "'Undo steps' edits are remembered.");
        ImGui::Separator();
        ImGui::SetNextItemWidth(215);
        ImGui::InputText("Settings file", mSettingsPath,
                         IM_ARRAYSIZE(mSettingsPath));
        ImGui::SameLine();
        if (ImGui::Button("Save as default")) { saveEditorSettings(); }
    }
    ImGui::End();
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
    ImGui::Separator();
    ImGui::BeginDisabled();
    toolButton("Edit", Tool::Edit);
    ImGui::EndDisabled();
    ImGui::Separator();

    if (mActiveTool == Tool::Resize)
    {
        ImGui::TextWrapped("Resize-mode: hover to highlight, click to "
                           "select, drag inside to move, handles to resize. "
                           "Click the tool again to turn it off.");
    }
    else if (mActiveTool == Tool::Rotate)
    {
        ImGui::TextWrapped("Rotate-mode: click to select, drag a corner "
                           "arrow to rotate around the pivot, drag the pivot "
                           "marker to move it within the object. "
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

/// Builds the level dropdown string with custom names
/// Only includes levels that have corresponding JSON files in assets/levels/
/// The dropdown index corresponds to the ELevel value (1-10) for proper selection
std::string
Editor::buildLevelDropdownString() const
{
    // Return cached result if available
    if (!mLevelDropdownDirty && !mLevelDropdownCache.empty())
    {
        return mLevelDropdownCache;
    }
    
    std::string result;
    result.append("None");
    result.push_back('\0');
    
    namespace fs = std::filesystem;
    const std::string directory = "assets/levels";
    
    std::error_code ec;
    std::string actualDirectory = directory;
    
    if (!fs::exists(directory, ec)) {
        // Try alternative path (parent directory's assets)
        std::string altPath = "../assets/levels";
        if (fs::exists(altPath, ec)) {
            actualDirectory = altPath;
        } else {
            // Try to create the directory if it doesn't exist
            fs::create_directories(directory, ec);
            if (fs::exists(directory, ec)) {
                actualDirectory = directory;
            } else {
                // If directory still doesn't exist, return just "None"
                return result;
            }
        }
    }
    
    // Build a map of ELevel -> display name by scanning all JSON files
    std::map<int, std::string> levelToDisplayName;
    
    // Regex to match standard level files (level1.json, level2.json, etc.)
    const std::regex levelFileRe("^level(\\d+)\\.json$", std::regex_constants::icase);
    
    // First pass: read all JSON files and build the mapping
    for (const auto &entry : fs::directory_iterator{ actualDirectory, ec })
    {
        if (ec) { break; }
        
        const std::string filename = entry.path().filename().string();
        
        // Only process .json files
        if (entry.path().extension() != ".json") { continue; }
        
        try {
            int eLevelValue = -1;
            std::string displayName;
            
            // Try to read the JSON file
            std::ifstream f(entry.path());
            if (!f.is_open()) { continue; }
            
            json data;
            f >> data;
            
            // Try to get eLevel from JSON
            if (data.contains("eLevel"))
            {
                eLevelValue = data["eLevel"].get<int>();
            }
            
            // If eLevel is not found or invalid, try to parse from filename
            if (!ELevel::_is_valid(eLevelValue))
            {
                std::smatch match;
                if (std::regex_match(filename, match, levelFileRe))
                {
                    eLevelValue = std::stoi(match[1]);
                }
                else
                {
                    // Custom-named file without eLevel in JSON, skip it
                    continue;
                }
            }
            
            if (!ELevel::_is_valid(eLevelValue)) { continue; }
            
            // Get display name: custom name from JSON, or from our map, or default
            if (data.contains("levelName"))
            {
                displayName = data["levelName"].get<std::string>();
            }
            else
            {
                // Use custom name from map if available
                ELevel level = ELevel::_from_integral(eLevelValue);
                auto customIt = mCustomLevelNames.find(level);
                if (customIt != mCustomLevelNames.end() && !customIt->second.empty())
                {
                    displayName = customIt->second;
                }
                else
                {
                    // Default to "LevelN"
                    displayName = ELevel::_from_integral(eLevelValue)._to_string();
                }
            }
            
            // Store the mapping
            levelToDisplayName[eLevelValue] = displayName;
        }
        catch (const std::exception &e) {
            // Skip files that can't be parsed
            continue;
        }
    }
    
    // Second pass: add levels in order (1-10) if they have files
    // This ensures dropdown index matches ELevel value
    for (int i = 1; i <= 10; ++i)
    {
        if (ELevel::_is_valid(i))
        {
            auto it = levelToDisplayName.find(i);
            if (it != levelToDisplayName.end())
            {
                result += it->second;
                result += '\0';
            }
        }
    }
    
    // Cache the result (use const_cast to modify mutable member)
    const_cast<std::string&>(mLevelDropdownCache) = result;
    const_cast<bool&>(mLevelDropdownDirty) = false;
    
    return result;
}

void
Editor::drawNewLevelDialog()
{
    if (!mShowNewLevelDialog) { return; }
    
    ImGui::OpenPopup("Create New Level");
    if (ImGui::BeginPopupModal("Create New Level", nullptr,
                               ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Enter the name for the new level:");
        ImGui::InputText("##newLevelName", mNewLevelName, sizeof(mNewLevelName));
        
        if (ImGui::Button("Create"))
        {
            createNewLevel();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            mShowNewLevelDialog = false;
            mNewLevelName[0] = '\0';
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
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

    // clicks/motion over the editor widget stay with ImGui
    if (ImGui::GetIO().WantCaptureMouse)
    {
        mWorld->setHoverHighlight(b2_nullBodyId);
        return;
    }

    // the transform tools (resize/rotate) gate all selection interactions
    if (mActiveTool != Tool::Resize && mActiveTool != Tool::Rotate)
    {
        mWorld->setHoverHighlight(b2_nullBodyId);
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
    // the text belongs purely to the active tool state, not to an object
    if (mActiveTool != Tool::Resize && mActiveTool != Tool::Rotate) { return; }

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
    ImGui::Text("%s", mActiveTool == Tool::Rotate
                        ? "rotate-mode active" : "resize-mode active");
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
    ImGui::Combo("Texture", &mEditTextureId, kTextureNames, kNumTextures);
    ImGui::Combo("EntityType", &mEditEntityType, kEntityTypeNames, kNumEntityTypes);
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
        mRenameActive = false;
        mShowEditorSettings = false;
        return;
    }
    
    // Load custom level names from JSON files
    if (mCustomLevelNames.empty() && mWorld != nullptr)
    {
        loadCustomLevelNames();
        // Invalidate dropdown cache since we loaded new level names
        mLevelDropdownDirty = true;
    }
    
    // Sync selected level with current active level when editor is opened
    if (*p_open && mWorld != nullptr && mSelectedLevel != mWorld->getActiveLevel())
    {
        mSelectedLevel = mWorld->getActiveLevel();
        fmt::print("LevelEditor: synced to current active level: {}\n", mSelectedLevel._to_string());
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

        // Level switching - use dynamic dropdown with custom names
        int levelItem = mSelectedLevel._to_integral();
        std::string dropdownString = buildLevelDropdownString();
        if (ImGui::Combo("Level", &levelItem, dropdownString.c_str()))
        {
            if (ELevel::_is_valid(levelItem) && ELevel::_from_integral(levelItem) != mSelectedLevel)
            {
                switchLevel(ELevel::_from_integral(levelItem));
            }
        }
        
        ImGui::SameLine();
        if (ImGui::Button("Create New"))
        {
            mShowNewLevelDialog = true;
            mNewLevelName[0] = '\0';
        }
        
        // Draw the new level dialog
        drawNewLevelDialog();

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
                    if (transformModeActive())
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
                                              transformModeActive());
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
    drawEditorSettings();
}

} // namespace RyuLevel
