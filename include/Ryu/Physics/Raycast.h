#pragma once

#include <box2d/box2d.h>

/*
* Result of a single closest-hit raycast against the physics world.
* Filled in by Ryu::Physics::createRaycast (b2World_CastRay).
*/
class RayCastClosest
{
  public:
    RayCastClosest()
      : m_Hit(false), m_Point({0, 0}), m_Normal({0, 0}), m_fraction(0)
    {}

    bool m_Hit;
    b2Vec2 m_Point;
    b2Vec2 m_Normal;
    float m_fraction;

    bool getHit() const { return m_Hit; }
    b2Vec2 getHitPoint() const { return m_Point; }
    b2Vec2 getHitNormal() const { return m_Normal; }
    float getFraction() const { return m_fraction; }
};
