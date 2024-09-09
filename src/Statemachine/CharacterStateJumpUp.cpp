#include "Ryu/Statemachine/CharacterStateLedgeHang.h"
#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Statemachine/CharacterState.h>
#include <Ryu/Statemachine/CharacterStateDuckEnter.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterStateJumpUp.h>

#include <fmt/core.h>

#include <iostream>
#include <memory>

// namespace ryu{

CharacterStateJumpUp::CharacterStateJumpUp() {}

CharacterStateJumpUp::~CharacterStateJumpUp() {}

std::unique_ptr<CharacterState>
CharacterStateJumpUp::handleInput(CharacterBase &character, EInput input)
{
    switch (input)
    {
    case EInput::PressLeft:
    case EInput::PressRight:
    case EInput::PressUp:
    case EInput::PressDown: {

        return nullptr;
    }
    default:
        break;
    }

    return nullptr;
}

void
CharacterStateJumpUp::onNotify(CharacterBase &character, Ryu::EEvent event)
{
    fmt::print("onNotify CharacterStateJumpUp\n");
    switch (event._value)
    {
    case Ryu::EEvent::CharacterStartJump: {
        character.jumpUp();
        break;
    }
    }
}

void
CharacterStateJumpUp::update(CharacterBase &character)
{

    if (character.getSpriteAnimation().isFinished())
    {
        // FIXME: hang state transition tmp disabled
        // std::unique_ptr<CharacterStateLedgeHang> state =
        //    std::make_unique<CharacterStateLedgeHang>();
        std::unique_ptr<CharacterStateIdle> state
            = std::make_unique<CharacterStateIdle>();

        character.changeState(std::move(state));
    }
}

void
CharacterStateJumpUp::enter(CharacterBase &character)
{
    character.setupAnimation(Textures::CharacterID::IchiJumpUp);
    character.setCharacterStateEnum(ECharacterState::JumpUp);
}

void
CharacterStateJumpUp::exit(CharacterBase &character)

{
    character.getSpriteAnimation().restart();
}

//} /// namespace ryu
