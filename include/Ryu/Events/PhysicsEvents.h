#pragma once

#include "Ryu/Core/AssetIdentifiers.h"
#include <box2d/box2d.h>
#include <string>
#include <cstdint>

struct PhysicsObjectCreatedEvent {
    b2BodyId bodyId;
    b2ShapeId shapeId;
    std::string name;
    b2Vec2 position; // in meters
    b2Vec2 size; // in meters
    bool isDynamic;
    // legacy direct asset reference (e.g. crate physics asset);
    // empty -> scene-texture-registry lookup via textureName/tile
    std::variant<Textures::PhysicAssetsID, Textures::SceneID, Textures::SpritesheetID> textureId;
    // scene object: registry id + optional tile of a tileset (-1/-1 = whole image)
    std::string textureName;
    int tileCol = -1;
    int tileRow = -1;
    int layer = 1; // scenegraph layer (0=Background, 1=Ground1, 2=Foreground)
    float rotation = 0.f; // radians, ccw (scene objects)
    b2Vec2 visualOffset = {0.f, 0.f}; // meters, visual-only sprite shift
};

struct PhysicsObjectUpdatedEvent {
    b2BodyId bodyId;
    b2Vec2 position;
    std::string name;
    float rotation;
};

struct PhysicsObjectDestroyedEvent {
    b2BodyId bodyId;
    std::string name;
};


namespace CollisionCategories {
    constexpr uint64_t STATIC   = 0x0001;  // Ground, walls
    constexpr uint64_t DYNAMIC  = 0x0002;  // Crates, characters
    constexpr uint64_t PLAYER   = 0x0004;  // Player character
    constexpr uint64_t SENSOR   = 0x0008;  // Sensor shapes (e.g., for triggers)
}
