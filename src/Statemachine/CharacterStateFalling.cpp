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
    :  touchedFloor(false)
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
      if(character.getHit(RaycastPosition::Below))
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
