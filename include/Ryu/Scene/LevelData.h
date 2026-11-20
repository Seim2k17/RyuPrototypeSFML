/**
 * Generic level-data storage for scene objects: replaces the hardcoded
 * sceneObjects map from Physics.h.
 *
 * One JSON file per level lives in assets/levels/ (level1.json, level2.json, ...).
 * All coordinates in the files are in PIXELS; SceneObjectPhysicsParameters'
 * ctor converts px -> meters, same as with the old hardcoded initializer.
 *
 * @author: Simon Nguyen
 * @date: 02/02/2025
 */

#pragma once

#include <Ryu/Physics/Physics.h>
#include <Ryu/Scene/Parallax.h>
#include <Ryu/Scene/SceneEnums.h>

#include <array>
#include <map>
#include <string>
#include <vector>

namespace RyuLevel
{

using SceneObjects = std::vector<SceneObjectPhysicsParameters>;


/** Loads every levelN.json in \p directory, mapped by the N in the filename. */
std::map<ELevel, SceneObjects> loadLevelDataFiles(const std::string &directory = "assets/levels");

/** Parallax factors of every levelN.json ("parallax" root key, defaults
    kParallaxDefaults when the key is missing). */
std::map<ELevel, ParallaxFactors> loadParallaxData(const std::string &directory = "assets/levels");

/** Custom (display) names of every levelN.json ("levelName" root key);
    levels without the key fall back to the ELevel enum name. */
std::map<ELevel, std::string> loadLevelNames(const std::string &directory = "assets/levels");

/** Saves \p objects to levelN.json, according to the \p level; \p parallax
    is written to the file root when not nullptr, \p levelName overrides
    the "levelName" root key when not nullptr. */
void saveLevelData(ELevel level, const SceneObjects &objects,
                   const std::string &directory = "assets/levels",
                   const ParallaxFactors *parallax = nullptr,
                   const std::string *levelName = nullptr);

} // namespace RyuLevel
