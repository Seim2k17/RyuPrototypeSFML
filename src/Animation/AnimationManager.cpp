#include "Ryu/Animation/AnimationData.h"
#include "Ryu/Animation/JsonParser.h"
#include "Ryu/Core/AssetIdentifiers.h"
#include <Ryu/Animation/AnimationManager.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <variant>
#include <utility>
#include <algorithm>
#include <vector>
#include <fmt/core.h>
#include <utility>
#include <algorithm>
#include <vector>


AnimationManager::AnimationManager()
{
    readAnimationConfigs();
    outputConfigs();
}

AnimationManager::~AnimationManager()
{
}

const RyuParser::Animation &
AnimationManager::getCharacterAnimationConfig(Textures::SpritesheetID spritesheet, Textures::CharacterID aniId)
{
    return spritesheetConfigs.at(spritesheet).animations.at(aniId);
}

void
AnimationManager::readAnimationConfigs()
{
    // read the config files in a deterministic (alphabetical) order: the
    // directory iteration order differs between filesystems/machines, which
    // silently changed which config won when several files share the same
    // Spritesheet id (e.g. per-level exports for Ichi64x64)
    std::vector<std::filesystem::path> configFiles{};
    for(auto const& dir : std::filesystem::directory_iterator{configPath})
    {
        configFiles.push_back(dir.path());
    }
    std::sort(configFiles.begin(), configFiles.end());

    for(auto const& path : configFiles)
    {
        fmt::print("AnimationConfigFile {} found: \n",path.c_str());

        json configJson = json::parse(readFile(path)); // TODO: Hm whats wrong here ??? cannot parse because  to/fromjson were overwritten ??
        RyuParser::JsonAnimations jAnis;
        jParser.getAnimationsFromJson(configJson, jAnis);

        fmt::print("Reading config for {} \n", jAnis.jsonName.c_str());

        // several config files can contribute animations to the SAME
        // spritesheet id (e.g. a level base config plus a file holding only
        // the ledge animations): merge the animations instead of keeping
        // only the first file's entry, so a missing animation no longer
        // depends on the file order (that was the ledge-grab segfault:
        // whichever config won the Ichi64x64 key, the ledge hang/climbup
        // entries were missing when a different file won on another
        // machine)
        auto configFound = spritesheetConfigs.find(jAnis.spritesheetId);
        if (configFound == spritesheetConfigs.end())
        {
            // add all the Anis for a spritesheet to config-map
            spritesheetConfigs.emplace(
                std::make_pair(jAnis.spritesheetId, jAnis)
            );
        }
        else
        {
            for(auto const& ani : jAnis.animations)
            {
                configFound->second.animations.insert_or_assign(ani.first,
                                                                  ani.second);
            }
        }
    }
    fmt::print("Reading configs done.\n");
}

// TODO: extend ?
void
AnimationManager::outputConfigs()
{
    for(const auto& config : spritesheetConfigs)
    {
        fmt::print("Config {}, Name {}, Spritesheet {} \n", config.first._to_string(), config.second.jsonName, config.second.spritesheetPath);
    }
}

void
AnimationManager::outputStoredAnimations()
{
    for(const auto& spritesheet : spritesheetConfigs)
    {
        fmt::print("Sheet: {}\n",spritesheet.first._to_string());
        for(const auto& ani : spritesheet.second.animations)
        {
            std::string aniId = std::visit([](auto&& cId){return cId._to_string();},ani.first);
            fmt::print("    {}\n",aniId);
        }
    }
}

// TODO: move to utility function ?
std::string
AnimationManager::readFile(fs::path path)
{
    // Open the stream to 'lock' the file.
    std::ifstream f(path, std::ios::in | std::ios::binary);

    // Obtain the size of the file.
    const auto sz = fs::file_size(path);

    // Create a buffer.
    std::string result(sz, '\0');

    // Read the whole file into the buffer.
    f.read(result.data(), sz);

    return result;
}
