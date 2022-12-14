#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterStateRun.h>
#include <Ryu/Statemachine/CharacterState.h>

#include <iostream>
#include <memory>
#include <type_traits>
#include <typeinfo>


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
       case EInput::PressDown:
       {
           // JUMP/DUCK
           //return new CharacterStateRun();    
       }
     
    default:
        break;
    }

    return nullptr;
}
     
void 
CharacterStateIdle::update(CharacterBase& character)
{
    //std::cout << "IDLE"<< std::endl;
}


void
CharacterStateIdle::enter(CharacterBase& character)
{
    character.setupAnimation({
                .frameSize={80,96}
               ,.startFrame={0,0}
               ,.numFrames=1
               ,.duration = sf::seconds(1)
               ,.repeat = true
               ,.animationId = Textures::CharacterID::IchiIdleRun});
}

void
CharacterStateIdle::exit(CharacterBase& character)
{

}

//} /// namespace ryu