#include "Ryu/Control/CharacterEnums.h"
#include "Ryu/Core/Utilities.h"
#include "Ryu/Debug/b2DrawSFML.hpp"
#include "Ryu/Physics/RaycastTypes.h"
#include <Ryu/Physics/RaycastComponent.h>
#include <Ryu/Scene/SceneNode.h>
#include <box2d/box2d.h>

namespace Ryu::Physics {
namespace {

struct RaycastContext
{
    b2BodyId ignoreBody;
    RayCastClosest *result;
};

/// b2CastResultFcn: -1 filters the shape, returning the fraction clips the
/// ray so that the last reported hit is the closest one
float
raycastCallback(b2ShapeId shapeId, b2Vec2 point, b2Vec2 normal, float fraction,
                void *context)
{
    auto *ctx = static_cast<RaycastContext *>(context);

    if (not B2_IS_NULL(ctx->ignoreBody)
        && B2_ID_EQUALS(b2Shape_GetBody(shapeId), ctx->ignoreBody))
    {
        return -1.0f;
    }

    ctx->result->m_Hit = true;
    ctx->result->m_Point = point;
    ctx->result->m_Normal = normal;
    ctx->result->m_fraction = fraction;
    return fraction;
}

} // namespace

RaycastComponent::RaycastComponent() : rayCastCallbacks() {}

bool
RaycastComponent::getHit(RaycastPosition rcName)
{
    if (rayCastCallbacks.find(rcName) != rayCastCallbacks.end())
        return rayCastCallbacks.at(rcName).m_Hit;

    return false;
}

b2Vec2
RaycastComponent::getHitPoint(RaycastPosition rcName)
{
    if (rayCastCallbacks.find(rcName) != rayCastCallbacks.end())
        return rayCastCallbacks.at(rcName).m_Point;

    return {0, 0};
}

b2Vec2
RaycastComponent::getHitNormal(RaycastPosition rcName)
{
    if (rayCastCallbacks.find(rcName) != rayCastCallbacks.end())
        return rayCastCallbacks.at(rcName).m_Normal;

    return {0, 0};
}

float
RaycastComponent::getFraction(RaycastPosition rcName)
{
    if (rayCastCallbacks.find(rcName) != rayCastCallbacks.end())
        return rayCastCallbacks.at(rcName).m_fraction;

    return 0.f;
}

void
RaycastComponent::eraseRaycast(RaycastPosition rcName)
{
    rayCastPoints.erase(rcName);
    eraseRaycastPoints(rcName);
}

void
RaycastComponent::eraseRaycastPoints(RaycastPosition rcName)
{
    rayCastCallbacks.erase(rcName);
}

void
RaycastComponent::drawRaycasts(b2DrawSFML &debugDrawer)
{
    for (auto rc : rayCastPoints)
    {
        debugDrawer.DrawSegment(rc.second.first, rc.second.second,
                                b2HexColor::b2_colorBox2DRed);

        auto result = rayCastCallbacks.find(rc.first);
        if (result != rayCastCallbacks.end() && result->second.m_Hit)
        {
            debugDrawer.DrawPoint(result->second.m_Point, 5.f,
                                  b2HexColor::b2_colorBox2DGreen);
            debugDrawer.DrawSegment(
                result->second.m_Point,
                result->second.m_Point + result->second.m_Normal * 0.25f,
                b2HexColor::b2_colorBox2DYellow);
        }
    }
}

void
RaycastComponent::eraseBelow()
{
    if (rayCastCallbacks.find(RaycastPosition::Below) != rayCastCallbacks.end())
        eraseRaycast(RaycastPosition::Below);
    rayCastPoints.erase(RaycastPosition::Below);
}

void
RaycastComponent::createCharacterRaycast(RaycastPosition position,
                                         float rcPositionX, float rcPositionY,
                                         float angle, float length,
                                         EMoveDirection moveDirection,
                                         b2WorldId physWorldId,
                                         b2BodyId ignoreBody)
{
    rayCastCallbacks[position] = Ryu::Physics::createRaycast(
        position, std::make_pair(rcPositionX, rcPositionY), angle, length,
        moveDirection, physWorldId, ignoreBody, rayCastPoints);
}

RayCastClosest
createRaycast(RaycastPosition localPosition,
              std::pair<double, double> startPointPixels, float angle,
              float length, EMoveDirection charMoveDirection,
              b2WorldId physWorld, b2BodyId ignoreBody,
              RaycastPoints &rayCastPoints)
{
    // creating a raycast from the characters position
    // 0° right / 90° down (+y) / 180° left / 270° up (-y)
    // direction according lookdir of character
    float raycastAngle = B2_PI * angle / 180.0f;
    float lengthMeter = Converter::pixelsToMeters<double>(length);
    b2Vec2 d(lengthMeter * cosf(raycastAngle),
             lengthMeter * sinf(raycastAngle));

    // horizontal rays are cast in the facing direction of the character
    bool rcLeftOrRight = (angle >= 180 || angle == 0);
    int dir
        = ((charMoveDirection == EMoveDirection::Left && rcLeftOrRight) ? -1
                                                                        : 1);

    b2Vec2 p1(Converter::pixelsToMeters<double>(startPointPixels.first),
              Converter::pixelsToMeters<double>(startPointPixels.second));

    b2Vec2 p2 = p1 + (dir * d);

    rayCastPoints[localPosition] = std::make_pair(p1, p2);

    RayCastClosest callback;
    RaycastContext context{ignoreBody, &callback};
    b2QueryFilter filter{.categoryBits = 0xFFFFFFFF,
                         .maskBits = 0xFFFFFFFF};
    b2World_CastRay(physWorld, p1, p2 - p1, filter, raycastCallback,
                    &context);

    return callback;
}
} // namespace Ryu::Physics
