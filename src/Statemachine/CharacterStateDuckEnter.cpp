#include <Ryu/Statemachine/CharacterStateDuckEnter.h>
#include <Ryu/Statemachine/CharacterStateDuckIdle.h>
#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterState.h>
#include <Ryu/Core/AssetIdentifiers.h>

#include <iostream>
#include <memory>


//namespace ryu{

CharacterStateDuckEnter::CharacterStateDuckEnter(){}

CharacterStateDuckEnter::~CharacterStateDuckEnter(){}

std::unique_ptr<CharacterState>
CharacterStateDuckEnter::handleInput(CharacterBase& character,EInput input)
{
    switch (input)
    {
       case EInput::ReleaseLeft:
       case EInput::ReleaseRight:
       {
       }
     
    dedault:
        break;
    }

    return nullptr;
}
     
void 
CharacterStateDuckEnter::update(CharacterBase& character)
{
    if(character.getSpriteAnimation().isFinished())
    {
        auto state = std::make_unique<CharacterStateDuckIdle>();
        character.changeState(std::move(state));
    }
}


void
CharacterStateDuckEnter::enter(CharacterBase& character)
{
    character.setupAnimation(Textures::CharacterID::IchiDuckEnter);
    character.setCharacterStateEnum(ECharacterState::DuckEnter);
    character.setDuckState(true);
    /*
    character.destroyPhysics();
    character.initPhysics();
    */
}

void
CharacterStateDuckEnter::exit(CharacterBase& character)
{
    character.getSpriteAnimation().restart();
}

//} /// namespace ryu
