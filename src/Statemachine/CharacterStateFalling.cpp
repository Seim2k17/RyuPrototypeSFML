#include "Ryu/Physics/RaycastTypes.h"
#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateFalling.h>
#include <Ryu/Statemachine/CharacterStateFallingEnd.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterState.h>
#include <Ryu/Core/Utilities.h>

#include <iostream>
#include <memory>
#include <type_traits>
#include <typeinfo>

class RayCastClosest;

//namespace ryu{

CharacterStateFalling::CharacterStateFalling()
    :  touchedFloor(false), restingTicks(0)
{
    std::cout << "Falling-cdor\n";
}

CharacterStateFalling::~CharacterStateFalling(){
    std::cout << "Falling-dtor\n";
}

std::unique_ptr<CharacterState>
CharacterStateFalling::handleInput(CharacterBase& character,EInput input)
{
    switch (input)
    {
       case EInput::PressDown:
       {
           // TODO: Fall down fast ?
           //return new CharacterStateRun();    
       }
     
    default:
        break;
    }

    return nullptr;
}
     
void 
CharacterStateFalling::update(CharacterBase& character)
{
    if(not touchedFloor)
    {
      // Raycast-based floor detection is currently not available
      // (createRaycasts is commented out pending the Box2D v3 port), so the
      // physics body is used instead: a body resting on the ground keeps a
      // vertical velocity of ~0, while a falling body under gravity gains
      // more than 1 m/s per tick and can never linger there. The condition
      // has to hold for a few consecutive ticks to be robust.
      static constexpr int RESTING_TICKS_REQUIRED = 3;
      b2Vec2 velocity = b2Body_GetLinearVelocity(character.getBody());
      bool restingOnGround =
          (velocity.y >= -0.01f && velocity.y <= 0.01f);

      if (restingOnGround)
      {
          ++restingTicks;
      }
      else
      {
          restingTicks = 0;
      }

      if (character.getHit(RaycastPosition::Below)
          || restingTicks >= RESTING_TICKS_REQUIRED)
      {
        touchedFloor = true;
        character.eraseRaycast(RaycastPosition::Below);
        std::unique_ptr<CharacterStateFallingEnd> state = std::make_unique<CharacterStateFallingEnd>();
        character.changeState(std::move(state));
      }
    }
}

void
CharacterStateFalling::enter(CharacterBase& character)
{
    character.setupAnimation(Textures::CharacterID::IchiFallingLoop);
    character.setCharacterStateEnum(ECharacterState::Falling);

    if(character.duckStateActive())
    {
        /*
        character.destroyPhysics();
        character.initPhysics();
        */
        character.setDuckState(false);
    }
}

void
CharacterStateFalling::exit(CharacterBase& character)
{
    if(character.getLinearVelocity().y < fallingTreshhold)
    {
        character.setActionHeight(EActionHeight::Low);
    }
    else
    {
        character.setActionHeight(EActionHeight::High);
    }
    character.eraseRaycastPoints(RaycastPosition::Below);
    character.getSpriteAnimation().restart();
}

//} /// namespace ryu
