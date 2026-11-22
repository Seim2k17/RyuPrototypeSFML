#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Statemachine/CharacterState.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterStateJumpNormal.h>
#include <Ryu/Statemachine/CharacterStateLedgeHang.h>

#include <fmt/core.h>

#include <memory>

// namespace ryu{

CharacterStateJumpNormal::CharacterStateJumpNormal()
    {}

CharacterStateJumpNormal::~CharacterStateJumpNormal() {}

std::unique_ptr<CharacterState>
CharacterStateJumpNormal::handleInput(CharacterBase &character, EInput input) {
    switch (input) {
    // TODO: check also for release keystates
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

void CharacterStateJumpNormal::onNotify(CharacterBase &character,
                                    Ryu::EEvent event) {
    fmt::print("onNotify CharacterStateJumpNormal\n");
    switch (event._value) {
    case Ryu::EEvent::CharacterStartJump: {
        character.jumpForward();
        break;
    }
    }
}

void CharacterStateJumpNormal::update(CharacterBase &character) {

    if (character.checkLedgeGrab()) {
        std::unique_ptr<CharacterStateLedgeHang> state =
            std::make_unique<CharacterStateLedgeHang>();
        character.changeState(std::move(state));
        return;
    }

    if (character.getSpriteAnimation().isFinished()) {

        // TODO: check if still running -> go back to running,
        // if no key is pressed -> either IDLE or better EndJump (impact etc...)
        std::unique_ptr<CharacterStateIdle> state =
            std::make_unique<CharacterStateIdle>();
        character.changeState(std::move(state));
    }
}

void CharacterStateJumpNormal::enter(CharacterBase &character) {
    character.setupAnimation(Textures::CharacterID::IchiJumpForwardNrml);
    character.setCharacterStateEnum(ECharacterState::JumpForward);
}

void CharacterStateJumpNormal::exit(CharacterBase &character)

{
    character.getSpriteAnimation().restart();
}

//} /// namespace ryu
