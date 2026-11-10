#include "Ryu/Scene/LevelData.h"

#include <Ryu/Core/Utilities.h>

#include <fmt/core.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <regex>

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

const std::map<std::string, Textures::SceneID> SceneIdByName = {
    { "BGMountain", Textures::SceneID::BGMountain },
    { "BoxPushable", Textures::SceneID::BoxPushable },
    { "BoxHeavy", Textures::SceneID::BoxHeavy },
    { "Button", Textures::SceneID::Button },
    { "Grass", Textures::SceneID::Grass },
    { "Grate", Textures::SceneID::Grate },
    { "Ground", Textures::SceneID::Ground },
    { "Ladder", Textures::SceneID::Ladder },
    { "Teleport", Textures::SceneID::Teleport },
    { "Unknown", Textures::SceneID::Unknown },
};

Textures::SceneID
sceneIdFromString(const std::string &name)
{
    auto it = SceneIdByName.find(name);
    return it != SceneIdByName.end() ? it->second : Textures::SceneID::Unknown;
}

std::string
sceneIdToString(Textures::SceneID id)
{
    for (const auto &[name, sceneId] : SceneIdByName)
    {
        if (sceneId == id) { return name; }
    }
    return "Unknown";
}

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
    return {
        { obj.at("positionPx").at(0).get<float>(),
          obj.at("positionPx").at(1).get<float>() },
        { obj.at("sizePx").at(0).get<float>(),
          obj.at("sizePx").at(1).get<float>() },
        obj.at("name").get<std::string>(),
        bodyTypeFromString(obj.at("bodyType").get<std::string>()),
        sceneIdFromString(obj.at("textureId").get<std::string>()),
        entityTypeFromString(obj.at("entityType").get<std::string>()),
        obj.value("layer", 1),
        // degrees in the JSON, radians in the physics world
        Converter::degToRad(obj.value("rotationDeg", 0.f))
    };
}

json
objectToJson(const SceneObjectPhysicsParameters &object)
{
    return {
        { "name", object.mName },
        { "positionPx", { Converter::metersToPixels(object.mPosition.x),
                          Converter::metersToPixels(object.mPosition.y) } },
        { "sizePx", { Converter::metersToPixels(object.mSize.x),
                      Converter::metersToPixels(object.mSize.y) } },
        { "bodyType", bodyTypeToString(object.mType) },
        { "textureId", sceneIdToString(object.mTextureId) },
        { "entityType", entityTypeToString(object.mEntityType) },
        { "layer", object.mLayer },
        { "rotationDeg", Converter::radToDeg(object.mRotation) }
    };
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
    return levels;
}

void
saveLevelData(ELevel level, const SceneObjects &objects,
              const std::string &directory)
{
    namespace fs = std::filesystem;

    std::error_code ec;
    fs::create_directories(directory, ec);

    json out = {
        { "levelName", level._to_string() },
        { "objects", json::array() }
    };
    for (const auto &object : objects) {
        out["objects"].push_back(objectToJson(object));
    }

    const auto path = levelFilePath(level, directory);
    std::ofstream file(path);
    file << out.dump(1);
    fmt::print("LevelData: saved {} objects to {}.\n", objects.size(), path.string());
}

} // namespace RyuLevel
