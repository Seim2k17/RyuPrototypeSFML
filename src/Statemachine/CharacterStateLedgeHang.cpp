#include <Ryu/Statemachine/CharacterStateLedgeHang.h>

#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateLedgeClimbUp.h>
#include <Ryu/Statemachine/CharacterStateFalling.h>
#include <Ryu/Statemachine/CharacterState.h>

#include <Ryu/Core/Utilities.h>
#include <box2d/box2d.h>
#include <memory>

// sprite anchoring derived from the 80x128 hang/climbup frames (see
// ICharacter.h): the frame center sits (40-35)=*dir px horizontally and
// (64-60) px below the raycast corner

CharacterStateLedgeHang::CharacterStateLedgeHang(){}

CharacterStateLedgeHang::~CharacterStateLedgeHang(){}

std::unique_ptr<CharacterState>
CharacterStateLedgeHang::handleInput(CharacterBase& character,EInput input)
{
    switch (input)
    {
       case EInput::PressLeft:
       case EInput::PressRight:
       {
           break;
       }

       case EInput::PressUp:
       {
           return std::move(std::make_unique<CharacterStateLedgeClimbUp>());
       }

       case EInput::PressDown:
       {
           return std::move(std::make_unique<CharacterStateFalling>());
       }

    default:
        break;
    }

    return nullptr;
}

void
CharacterStateLedgeHang::update(CharacterBase& character)
{
}


void
CharacterStateLedgeHang::enter(CharacterBase& character)
{
    auto bodyId = character.getBody();
    b2Body_SetGravityScale(bodyId, 0);
    b2Body_SetLinearVelocity(bodyId, {0.f, 0.f});

    character.setupAnimation(Textures::CharacterID::IchiLedgeHangIdle);
    character.setCharacterStateEnum(ECharacterState::Hanging);

    auto corner = character.getLedgeGrabPoint();
    float dir = character.getDirectionMultiplier();

    // hands grab at the ledge corner: the hands sit raycastOffset above the
    // body center, the body back rests on the wall face
    sf::Vector2f bodyPos{corner.x - dir * (INIT_FRAME_SIZE.first / 2.f),
                         corner.y + Ryu::Physics::raycastOffset};
    b2Body_SetTransform(bodyId,
                        {Converter::pixelsToMeters<float>(bodyPos.x),
                         Converter::pixelsToMeters<float>(bodyPos.y)},
                        b2Rot_identity);

    auto& spriteAni = character.getSpriteAnimation();
    // hang and climbup share one fixed frame camera, so both states anchor
    // the sprite the same way: the frame-internal ledge corner lands on
    // the wall corner (see ICharacter.h for the derivation)
    spriteAni.setAnimationPosition({
        // -2: the raycast corner points dir*2 INTO the wall, the hands
        // grip ON the visible corner (wall face at the ledge top)
        corner.x
            + dir * (LEDGE_FRAME_ANCHOR_X - LEDGE_FRAME_GRIP_X - 2.f),
        corner.y + (LEDGE_FRAME_ANCHOR_Y - LEDGE_FRAME_GRIP_Y)});
}

void
CharacterStateLedgeHang::exit(CharacterBase& character)
{
    character.getSpriteAnimation().restart();
    // releasing a hang (PressDown/other transitions to Falling) must not
    // instantly re-grab the SAME ledge while the hands are still in the
    // grab window - stay grab-blind for a moment
    character.setLedgeGrabCooldown(sf::milliseconds(300));
    auto bodyId = character.getBody();
    // restore the shared character gravity scale (see
    // CHARACTER_BODY_GRAVITY_SCALE - previously this restored 4.8 while
    // the body was created with 9.8, silently changing jump heights
    // depending on the state path)
    b2Body_SetGravityScale(bodyId, CHARACTER_BODY_GRAVITY_SCALE);
}
