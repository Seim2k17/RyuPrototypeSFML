#include "Ryu/Scene/LevelData.h"

#include <Ryu/Core/Utilities.h>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <regex>
#include <vector>

#include <algorithm>
#include <cctype>

namespace RyuLevel
{

namespace
{

namespace fs = std::filesystem;
using json = nlohmann::json;

b2BodyType
bodyTypeFromString(const std::string &type)
{
    if (type == "dynamic") { return b2_dynamicBody; }
    if (type == "kinematic") { return b2_kinematicBody; }
    return b2_staticBody;
}

std::string
bodyTypeToString(b2BodyType type)
{
    switch (type)
    {
    case b2_dynamicBody: return "dynamic";
    case b2_kinematicBody: return "kinematic";
    default: return "static";
    }
}

// texture ids are data-driven now (SceneTextureRegistry ids, stored as
// plain strings in the level JSONs); the former enum mapping is gone

EntityType
entityTypeFromString(const std::string &name)
{
    return name == "Climbable" ? EntityType::Climbable : EntityType::None;
}

std::string
entityTypeToString(EntityType type)
{
    return type == EntityType::Climbable ? "Climbable" : "None";
}

SceneObjectPhysicsParameters
objectFromJson(const json &obj)
{
    const auto tile = obj.value("tile", std::vector<int>{ -1, -1 });
    return {
        { obj.at("positionPx").at(0).get<float>(),
          obj.at("positionPx").at(1).get<float>() },
        { obj.at("sizePx").at(0).get<float>(),
          obj.at("sizePx").at(1).get<float>() },
        obj.at("name").get<std::string>(),
        bodyTypeFromString(obj.at("bodyType").get<std::string>()),
        obj.value("textureId", "Unknown"),
        entityTypeFromString(obj.at("entityType").get<std::string>()),
        obj.value("layer", 1),
        // degrees in the JSON, radians in the physics world
        Converter::degToRad(obj.value("rotationDeg", 0.f)),
        obj.value("hasPhysics", true),
        // tile of a tileset: [col, row]; missing/negative = whole image
        tile.at(0), tile.at(1)
    };
}

json
objectToJson(const SceneObjectPhysicsParameters &object)
{
    json out = {
        { "name", object.mName },
        { "positionPx", { Converter::metersToPixels(object.mPosition.x),
                          Converter::metersToPixels(object.mPosition.y) } },
        { "sizePx", { Converter::metersToPixels(object.mSize.x),
                      Converter::metersToPixels(object.mSize.y) } },
        { "bodyType", bodyTypeToString(object.mType) },
        { "textureId", object.mTextureId },
        { "entityType", entityTypeToString(object.mEntityType) },
        { "layer", object.mLayer },
        { "rotationDeg", Converter::radToDeg(object.mRotation) },
        { "hasPhysics", object.mHasPhysics }
    };
    if (object.mTileCol >= 0 and object.mTileRow >= 0)
    {
        out["tile"] = { object.mTileCol, object.mTileRow };
    }
    return out;
}

fs::path
levelFilePath(ELevel level, const std::string &directory)
{
    std::string levelName = level._to_string();
    std::transform(levelName.begin(), levelName.end(), levelName.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return fs::path(directory) / (levelName + ".json");
}

} // anonymous namespace

std::map<ELevel, SceneObjects>
loadLevelDataFiles(const std::string &directory)
{
    std::map<ELevel, SceneObjects> levels;
    const std::regex levelFileRe("^level(\\d+)\\.json$");

    std::error_code ec;
    if (not fs::exists(directory, ec)) {
        fmt::print("LevelData: level directory '{}' does not exist.\n", directory);
        return levels;
    }

    // First pass: load standard levelN.json files
    for (const auto &entry : fs::directory_iterator{ directory })
    {
        std::smatch match;
        const std::string filename = entry.path().filename().string();
        if (not std::regex_match(filename, match, levelFileRe)) { continue; }

        const auto levelId = std::stoi(match[1]);
        if (not ELevel::_is_valid(levelId)) {
            fmt::print("LevelData: unknown level id '{}' in {}.\n", levelId, filename);
            continue;
        }

        try {
            json data;
            std::ifstream f(entry.path());
            f >> data;

            SceneObjects objects;
            for (const auto &obj : data.at("objects")) {
                objects.push_back(objectFromJson(obj));
            }
            const auto objectCount = objects.size();
            levels.emplace(ELevel::_from_integral(levelId), std::move(objects));
            fmt::print("LevelData: loaded {} with {} objects.\n", filename, objectCount);
        }
        catch (const std::exception &e) {
            fmt::print("LevelData: failed to parse {}: {}.\n", filename, e.what());
        }
    }
    
    // Second pass: load custom-named .json files (e.g., jumpingLevel.json)
    for (const auto &entry : fs::directory_iterator{ directory })
    {
        const std::string filename = entry.path().filename().string();
        // Skip standard levelN.json files
        if (std::regex_match(filename, levelFileRe)) { continue; }
        // Only process .json files
        if (entry.path().extension() != ".json") { continue; }
        
        try {
            json data;
            std::ifstream f(entry.path());
            f >> data;

            // Check if the file specifies an eLevel
            int levelId = -1;
            if (data.contains("eLevel"))
            {
                levelId = data["eLevel"].get<int>();
            }
            else if (data.contains("level"))
            {
                levelId = data["level"].get<int>();
            }
            
            // If no eLevel specified, assign to next available (starting from 4)
            if (levelId < 1 || levelId > 10 || !ELevel::_is_valid(levelId))
            {
                // Find next available levelId starting from 4
                levelId = 4;
                while (levelId <= 10 && levels.find(ELevel::_from_integral(levelId)) != levels.end())
                {
                    levelId++;
                }
                if (levelId > 10)
                {
                    fmt::print("LevelData: too many custom levels, skipping {}.\n", filename);
                    continue;
                }
            }
            
            SceneObjects objects;
            for (const auto &obj : data.at("objects")) {
                objects.push_back(objectFromJson(obj));
            }
            const auto objectCount = objects.size();
            ELevel level = ELevel::_from_integral(levelId);
            levels.emplace(level, std::move(objects));
            fmt::print("LevelData: loaded custom level {} with {} objects from {}.\n", 
                       level._to_string(), objectCount, filename);
        }
        catch (const std::exception &e) {
            fmt::print("LevelData: failed to parse {}: {}.\n", filename, e.what());
        }
    }
    
    return levels;
}

std::map<ELevel, std::string>
loadLevelNames(const std::string &directory)
{
    std::map<ELevel, std::string> names;
    const std::regex levelFileRe("^level(\\d+)\\.json$");

    std::error_code ec;
    if (not fs::exists(directory, ec)) {
        return names;
    }

    for (const auto &entry : fs::directory_iterator{ directory })
    {
        std::smatch match;
        const std::string filename = entry.path().filename().string();
        if (not std::regex_match(filename, match, levelFileRe)) { continue; }
        const auto levelId = std::stoi(match[1]);
        if (not ELevel::_is_valid(levelId)) { continue; }

        try {
            json data;
            std::ifstream f(entry.path());
            f >> data;

            const auto level = ELevel::_from_integral(levelId);
            names.emplace(level,
                          data.value("levelName", level._to_string()));
        }
        catch (const std::exception &e) {
            fmt::print("LevelData: failed to parse name of {}: {}.\n",
                       filename, e.what());
        }
    }
    return names;
}

std::map<ELevel, ParallaxFactors>
loadParallaxData(const std::string &directory)
{
    std::map<ELevel, ParallaxFactors> parallax;
    const std::regex levelFileRe("^level(\\d+)\\.json$");

    std::error_code ec;
    if (not fs::exists(directory, ec)) {
        return parallax;
    }

    for (const auto &entry : fs::directory_iterator{ directory })
    {
        std::smatch match;
        const std::string filename = entry.path().filename().string();
        if (not std::regex_match(filename, match, levelFileRe)) { continue; }
        const auto levelId = std::stoi(match[1]);
        if (not ELevel::_is_valid(levelId)) { continue; }

        try {
            json data;
            std::ifstream f(entry.path());
            f >> data;

            ParallaxFactors factors = kParallaxDefaults;
            if (data.contains("parallax")) {
                const auto values = data.at("parallax");
                for (std::size_t i = 0; i < factors.size()
                     && i < values.size(); ++i) {
                    factors.at(i) = values.at(i).get<float>();
                }
            }
            const auto level
                = ELevel::_from_integral(levelId);
            parallax.emplace(level, factors);
        }
        catch (const std::exception &e) {
            fmt::print("LevelData: failed to parse parallax of {}: {}.\n",
                       filename, e.what());
        }
    }
    return parallax;
}

void
saveLevelData(ELevel level, const SceneObjects &objects,
              const std::string &directory, const ParallaxFactors *parallax,
              const std::string *levelName)
{
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::create_directories(directory, ec);

    json out = {
        { "levelName", levelName != nullptr ? *levelName
                                           : level._to_string() },
        { "objects", json::array() }
    };
    for (const auto &object : objects) {
        out["objects"].push_back(objectToJson(object));
    }
    if (parallax != nullptr) {
        out["parallax"] = { parallax->at(0), parallax->at(1),
                            parallax->at(2) };
    }

    const auto path = levelFilePath(level, directory);
    std::ofstream file(path);
    file << out.dump(1);
    fmt::print("LevelData: saved {} objects to {}.\n", objects.size(), path.string());
}

} // namespace RyuLevel
