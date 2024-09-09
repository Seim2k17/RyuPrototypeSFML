#pragma once

#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Debug/b2DrawSFML.hpp>
#include <Ryu/Physics/Raycast.h>
#include <box2d/box2d.h>
#include <map>
#include <memory>

using RaycastPoints=std::map<RaycastPosition, std::pair<b2Vec2,b2Vec2> >;
class RayCastClosest;

namespace Ryu::Physics{

class RaycastComponent
{
public:
    RaycastComponent();

    void createCharacterRaycast(RaycastPosition position, float rcPositionX, float rcPositionY,
                                float angle, float length, EMoveDirection moveDirection,
                                std::unique_ptr<b2World>& physWorld);

    bool getHit(RaycastPosition rcName);

    void eraseRaycast(RaycastPosition rcName);
    void eraseRaycastPoints(RaycastPosition rcName);
    void eraseBelow();
    void drawRaycasts(b2DrawSFML& debugDrawer);

private:
  std::map<RaycastPosition, RayCastClosest> rayCastCallbacks;
  std::map<RaycastPosition, std::pair<b2Vec2, b2Vec2>> rayCastPoints;
};

  constexpr double raycastOffset = 25.0f;
  RayCastClosest createRaycast(RaycastPosition positionLocal,
                               std::pair<double,double> startPoint,
                               float angle, float length,
                               EMoveDirection charMoveDirection,
                               std::unique_ptr<b2World>& physWorld,
                               RaycastPoints& rayCastPoints);
} // namespace Ryu::Physics
