#pragma once

#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Debug/b2DrawSFML.hpp>
#include <Ryu/Physics/Raycast.h>
#include <Ryu/Physics/RaycastTypes.h>
#include <box2d/box2d.h>
#include <map>
#include <memory>

using RaycastPoints = std::map<RaycastPosition, std::pair<b2Vec2, b2Vec2>>;
class RayCastClosest;

namespace Ryu::Physics {

class RaycastComponent
{
public:
    RaycastComponent();

    /// \brief  Creates and immediately casts a ray in the physics world.
    /// \param  position    logical name the ray is stored under (Up/Mid/...)
    /// \param  rcPositionX ray origin x in pixels (world space)
    /// \param  rcPositionY ray origin y in pixels (world space)
    /// \param  angle       ray direction in degrees (0 right / 90 down /
    ///                     180 left / 270 up)
    /// \param  length      ray length in pixels
    /// \param  moveDirection facing of the character, horizontal rays are
    ///                     cast in this direction
    /// \param  ignoreBody  physics body to ignore (the character itself),
    ///                     may be b2_nullBodyId
    void createCharacterRaycast(RaycastPosition position, float rcPositionX,
                                float rcPositionY, float angle, float length,
                                EMoveDirection moveDirection,
                                b2WorldId physWorldId, b2BodyId ignoreBody);

    bool getHit(RaycastPosition rcName);
    b2Vec2 getHitPoint(RaycastPosition rcName);
    b2Vec2 getHitNormal(RaycastPosition rcName);
    float getFraction(RaycastPosition rcName);

    void eraseRaycast(RaycastPosition rcName);
    void eraseRaycastPoints(RaycastPosition rcName);
    void eraseBelow();
    void drawRaycasts(b2DrawSFML &debugDrawer);

private:
    std::map<RaycastPosition, RayCastClosest> rayCastCallbacks;
    std::map<RaycastPosition, std::pair<b2Vec2, b2Vec2>> rayCastPoints;
};

constexpr double raycastOffset = 25.0f;

/// \brief  Casts a single ray (closest hit) against the world and stores the
///         result in result. The ray endpoints (in meters) are stored in
///         rayCastPoints for debug drawing.
/// \return the raycast result
RayCastClosest createRaycast(RaycastPosition localPosition,
                             std::pair<double, double> startPointPixels,
                             float angle, float length,
                             EMoveDirection charMoveDirection,
                             b2WorldId physWorld, b2BodyId ignoreBody,
                             RaycastPoints &rayCastPoints);
} // namespace Ryu::Physics
