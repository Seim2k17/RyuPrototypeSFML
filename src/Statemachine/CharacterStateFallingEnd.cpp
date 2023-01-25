#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateFallingEnd.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterState.h>
#include <Ryu/Core/Utilities.h>

#include <iostream>
#include <memory>


//namespace ryu{

CharacterStateFallingEnd::CharacterStateFallingEnd()
    :  touchedFloor(false)
{
    std::cout << "FallingEnd-cdor\n";
}

CharacterStateFallingEnd::~CharacterStateFallingEnd(){
    std::cout << "FallingEnd-dtor\n";
}

std::unique_ptr<CharacterState>
CharacterStateFallingEnd::handleInput(CharacterBase& character,EInput input)
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
CharacterStateFallingEnd::update(CharacterBase& character)
{
    if(character.getSpriteAnimation().isFinished())
    {
       std::unique_ptr<CharacterStateIdle> state = std::make_unique<CharacterStateIdle>();
       character.changeState(std::move(state));
    }
}

void
CharacterStateFallingEnd::enter(CharacterBase& character)
{
    if(character.getActionHeight() == EActionHeight::Low)
    {
        character.setupAnimation({
            .frameSize={80,96}
           ,.startFrame={4,1}
           ,.numFrames=14
           ,.duration = sf::milliseconds(1350)
           ,.repeat = false
           ,.animationId = Textures::CharacterID::IchiEndFallingLow});
    }
    
    if(character.getActionHeight() == EActionHeight::High)
    {
        character.setupAnimation({
            .frameSize={80,96}
           ,.startFrame={5,2}
           ,.numFrames=14
           ,.duration = sf::milliseconds(1550)
           ,.repeat = false
           ,.animationId = Textures::CharacterID::IchiEndFallingHigh});
    }
    
    character.setCharacterStateEnum(ECharacterState::FallingEnd);
}

void
CharacterStateFallingEnd::exit(CharacterBase& character)
{
    character.getSpriteAnimation().restart();
}

//} /// namespace ryu