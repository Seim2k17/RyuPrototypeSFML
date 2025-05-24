#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterStateJumpUp.h>
#include <Ryu/Statemachine/CharacterStateDuckEnter.h>
#include <Ryu/Statemachine/CharacterStateRun.h>
#include <Ryu/Statemachine/CharacterState.h>

#include <iostream>
#include <memory>


//namespace ryu{

CharacterStateIdle::CharacterStateIdle(){}

CharacterStateIdle::~CharacterStateIdle(){}

std::unique_ptr<CharacterState>
CharacterStateIdle::handleInput(CharacterBase& character,EInput input)
{
    switch (input)
    {
       case EInput::PressLeft:
       case EInput::PressRight:
       {
           return std::move(std::make_unique<CharacterStateRun>());    
       }

       case EInput::PressUp:
       {
           return std::move(std::make_unique<CharacterStateJumpUp>());    
       }

       case EInput::PressDown:
       {
           return std::move(std::make_unique<CharacterStateDuckEnter>());    
       }
     
    default:
        break;
    }

    return nullptr;
}
     
void 
CharacterStateIdle::update(CharacterBase& character)
{
}


void
CharacterStateIdle::enter(CharacterBase& character)
{
    //TODO: tmp normally its done in the exit state here we test it for climbup
    //character.getSpriteAnimation().restart();
    //character.getSpriteAnimation().setOrigin(0,0);
    auto& spriteAni = character.getSpriteAnimation();
    auto pivot = spriteAni.getPivotAbs();
    auto posSprite = spriteAni.getPosition();
    auto origSprite = spriteAni.getOrigin();
    fmt::print("Idle - Pivot: {}/{}\n", pivot.x, pivot.y);
    fmt::print("Idle - getSpritePosi: {}/{}\n", posSprite.x, posSprite.y);
    fmt::print("Idle - getOrigin: {}/{}\n", origSprite.x, origSprite.y);
    //spriteAni.setPosition((float)pivot.x/10,(float)pivot.y/10 );
    character.setupAnimation(Textures::CharacterID::IchiIdle);
    character.setCharacterStateEnum(ECharacterState::Idle);

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
CharacterStateIdle::exit(CharacterBase& character)
{
    character.getSpriteAnimation().restart();
}

//} /// namespace ryu
