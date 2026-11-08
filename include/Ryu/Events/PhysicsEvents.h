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
    std::variant<Textures::PhysicAssetsID, Textures::SceneID, Textures::SpritesheetID> textureId;  // Optional: Pass texture ID for rendering
    int layer = 1; // scenegraph layer (0=Background, 1=Ground1, 2=Foreground)
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
