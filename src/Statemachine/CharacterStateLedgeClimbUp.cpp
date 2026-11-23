#include "Ryu/Control/CharacterEnums.h"
#include "Ryu/Core/Utilities.h"
#include <Ryu/Statemachine/CharacterStateLedgeClimbUp.h>

#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Statemachine/CharacterState.h>

#include <SFML/System/Vector2.hpp>
#include <box2d/box2d.h>
#include <memory>

// how far the character ends up on the ledge after the climb (pixels from
// the corner); the sprite anchoring is shared with the hang state (see
// ICharacter.h, both states use the same fixed frame camera)
constexpr float CLIMBUP_STAND_X_OFFSET = 10.f;

CharacterStateLedgeClimbUp::CharacterStateLedgeClimbUp(){}

CharacterStateLedgeClimbUp::~CharacterStateLedgeClimbUp(){}

std::unique_ptr<CharacterState>
CharacterStateLedgeClimbUp::handleInput(CharacterBase& character,EInput input)
{
    switch (input)
    {
       case EInput::PressLeft:
       case EInput::PressRight:
       case EInput::PressUp:
       case EInput::ActionInput:
       default:
        break;
    }

    return nullptr;
}

void
CharacterStateLedgeClimbUp::update(CharacterBase& character)
{
    if (character.getSpriteAnimation().getCurrentFrame()
        >= character.getSpriteAnimation().getFramesCount())
    {
        auto corner = character.getLedgeGrabPoint();
        float dir = character.getDirectionMultiplier();

        sf::Vector2f bodyPos{
            corner.x + dir * CLIMBUP_STAND_X_OFFSET,
            corner.y - (INIT_FRAME_SIZE.second / 2.f)};
        auto bodyId = character.getBody();
        b2Body_SetTransform(bodyId,
                            {Converter::pixelsToMeters<float>(bodyPos.x),
                             Converter::pixelsToMeters<float>(bodyPos.y)},
                            b2Rot_identity);
        b2Body_SetLinearVelocity(bodyId, {0.f, 0.f});

        // sync the sprite to the teleported body IMMEDIATELY: the tick
        // order in CharacterBase::update is position-sync BEFORE the
        // state update, so without this the character rendered one tick
        // at the old climb-up position after the teleport (flicker)
        character.getSpriteAnimation().setAnimationPosition(
            character.animationPositionFromBody(b2Body_GetPosition(bodyId)));

        character.changeState(std::move(std::make_unique<CharacterStateIdle>()));
    }
}


void
CharacterStateLedgeClimbUp::enter(CharacterBase& character)
{
    auto bodyId = character.getBody();
    b2Body_SetGravityScale(bodyId, 0);
    b2Body_SetLinearVelocity(bodyId, {0.f, 0.f});

    auto& spriteAni = character.getSpriteAnimation();
    character.setupAnimation(Textures::CharacterID::IchiLedgeClimbUp);
    character.setCharacterStateEnum(ECharacterState::ClimbUp);

    auto corner = character.getLedgeGrabPoint();
    float dir = character.getDirectionMultiplier();
    // same anchor as the hang state (one shared frame camera): frame-internal
    // ledge corner onto the wall corner, the climb motion happens inside the
    // frames
    spriteAni.setAnimationPosition({
        // -2: the raycast corner points dir*2 INTO the wall, the hands
        // grip ON the visible corner (wall face at the ledge top)
        corner.x
            + dir * (LEDGE_FRAME_ANCHOR_X - LEDGE_FRAME_GRIP_X - 2.f),
        corner.y + (LEDGE_FRAME_ANCHOR_Y - LEDGE_FRAME_GRIP_Y)});
}

void
CharacterStateLedgeClimbUp::exit(CharacterBase& character)
{
    character.getSpriteAnimation().restart();
    auto bodyId = character.getBody();
    // restore the shared character gravity scale (see
    // CHARACTER_BODY_GRAVITY_SCALE - previously this restored 4.8 while
    // the body was created with 9.8, silently changing jump heights
    // depending on the state path)
    b2Body_SetGravityScale(bodyId, CHARACTER_BODY_GRAVITY_SCALE);
}
