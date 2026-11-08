#include "Ryu/Animation/JsonParser.h"
#include "Ryu/Animation/SpritesheetAnimation.h"
#include "Ryu/Control/CharacterEnums.h"
#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Statemachine/CharacterStateLedgeClimbUp.h"
#include "Ryu/Statemachine/CharacterStateLedgeHang.h"
#include <Ryu/Animation/AnimationManager.h>
#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Physics/Physics.h>
#include <Ryu/Physics/RaycastTypes.h>
#include <Ryu/Statemachine/CharacterStateFalling.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>

#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/Shape.hpp>
#include <SFML/System/Angle.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <Thirdparty/glm/glm.hpp>
#include <cmath>
#include <fmt/core.h>

#include <box2d/box2d.h>

#include <algorithm>
#include <iostream>
#include <memory>

// namespace ryu {

template <typename T>
bool
IsInBounds(const T &value, const T &low, const T &high)
{
    return !(value < low) && !(high < value);
}

CharacterBase::CharacterBase(const sf::Vector2i &position)
    : Observer("CharacterBase"),
      mCharacterState(std::make_unique<CharacterStateIdle>()),
      movement(0.f, 0.f),
      mMoveDirection(EMoveDirection::Right),
      mCharacterFalling(false),
      baseTextureManager(),
      mCharSettings(),
      mCurrentLevel(Textures::LevelID::Level1),
      mCharacterAnimation(this),
      mECharacterState(ECharacterState::None),
      mDuckStateActive(false),
      mPhysics(nullptr),
      mAnimationManager(std::make_unique<AnimationManager>()),
      mCharacterPhysicsValues({}),
      contactListener()
{
    // std::shared_ptr<CharacterBase> sPtr =
    // std::make_shared<CharacterBase>(this); mCharacterAnimation =
    // SpritesheetAnimation(std::move(sPtr));
    loadTextures();
}

void
CharacterBase::setPhysicWorldRef(std::unique_ptr<b2World> &phWorld)
{
    // TODO: check if this makes sense this way ? -> the owner of the physicsworld should be World
    // move contactlistener to Physicsclas ?s
    // phWorldRef = std::move(phWorld);
    // phWorldRef->SetContactListener(&contactListener);

}

CharacterBase::CharacterBase(ECharacterState startState,
                             const sf::Vector2i &position)
    : Observer("CharacterBase"),
      mECharacterState(startState),
      mCharacterState(std::make_unique<CharacterStateIdle>()),
      mCharacterSpeed(55.0f), // startvalue playerspeed
      mMoveDirection(EMoveDirection::Right),
      mCharacterFalling(false),
      mActionHeight(EActionHeight::Normal),
      mECharacterMovement(ECharacterMovement::CanClimb),
      baseTextureManager(),
      mCharSettings(),
      mCurrentLevel(Textures::LevelID::Level1),
      mDuckStateActive(false),
      mPhysics(nullptr),
      mSetOffset(false),
      mCharacterAnimation(this),
      mAnimationManager(std::make_unique<AnimationManager>()),
      contactListener()
{
    // std::shared_ptr<CharacterBase> sPtr =
    // std::make_shared<CharacterBase>(this); mCharacterAnimation =
    // SpritesheetAnimation(std::move(sPtr));
    // TODO: check if its needable&possible to start character from a certain
    // state
    loadTextures();
}

// TODO: moved to CharacterPhysicsParameter
void
CharacterBase::setCharacterSettings(CharacterSetting settings)
{
    // TODO: GUI related (DEBUG)
/*
    fmt::print("useDebugValues: JF: {}/{}, JU: {}/{}\n",
               settings.jumpForwardImpulse.x, settings.jumpForwardImpulse.y,
               settings.jumpUpImpulse.x, settings.jumpUpImpulse.y);

    mCharSettings.jumpForwardImpulse = settings.jumpForwardImpulse;
    mCharSettings.jumpUpImpulse
        = settings.jumpUpImpulse; // TODO: probably we need to convert st here;
*/
}

void
CharacterBase::setPositionOffset(sf::Vector2f offset)
{
    /*
        if (positionCrossOffset.x == 0 && positionCrossOffset.y == 0)
       positionCrossOffset = mCharacterAnimation.getPosition();

        mCharacterAnimation.setPosition({positionCrossOffset.x+offset.x,positionCrossOffset.y+offset.y});
    */
    lastPositionCrossOffset = positionCrossOffset;
    positionCrossOffset.x = offset.x;
    positionCrossOffset.y = offset.y;
    fmt::print("PosiCross Char: {}/{} \n Offset: {}/{}\n",
               positionCrossOffset.x, positionCrossOffset.y, offset.x,
               offset.y);
}

void
CharacterBase::resetCharacterSettings()
{
    // TODO: find better way for resetting stuff for DEBUG UI
    // see also Physics class
    fmt::print("reset Values");
    /*
    mCharSettings.MoveMultiplierX = mFinalCharSettings.MoveMultiplierX;
    mCharSettings.MoveMultiplierY = mFinalCharSettings.MoveMultiplierY;
    mCharSettings.jumpUpImpulse = mFinalCharSettings.jumpUpImpulse;
    mCharSettings.jumpForwardImpulse = mFinalCharSettings.jumpForwardImpulse;
    mCharSettings.bodyMass = mFinalCharSettings.bodyMass;
    mCharSettings.massCenter = mFinalCharSettings.massCenter;

    positionCrossOffset.x = 0.f;
    positionCrossOffset.y = 0.f;
    */
}
/*
void
CharacterBase::updatePhysics()
{
    sf::Vector2f position = mCharacterAnimation.getPosition();

    // initPhysics(phWorldRef,position);
}

void
CharacterBase::updatePhysics(const sf::Vector2f &position)
{
    // initPhysics(phWorldRef,position);
}
*/
// inits the physics at the current character position, used afte the initial
// state is set
/*
void
CharacterBase::initPhysics()
{
    // TODO: init Physics from world, delegating to Physics::initCharacterPhysics
    // initPhysics(phWorldRef, mCharacterAnimation.getPosition());

    // TODO: this flag should be set when we get a message back from the physics class
    physicsInitialized = true;
}
*/
void
CharacterBase::loadTextures()
{
    // just a transparent texture 32x32
    baseTextureManager.load(Textures::PhysicAssetsID::Empty,
                            "assets/scenes/99_dummy/box_empty.png");
}

float
CharacterBase::getDirectionMultiplier()
{
    return (getMoveDirection() == EMoveDirection::Left ? -1.0 : 1.0);
}

void
CharacterBase::jumpForward()
{
    // TODO: physicsrelated stuff now in the PhysicsClass please move this also
    /*
    fmt::print("JumpForward: x:{} y:{}\n", mCharSettings.jumpForwardImpulse.x,
               mCharSettings.jumpForwardImpulse.y);

    b2MassData mass{.mass = mCharSettings.bodyMass,
                    .center = mCharSettings.massCenter,
                    .I = 0};
    mBody->SetMassData(&mass);
    fmt::print("dirMultiplier: {}\n", getDirectionMultiplier());
    fmt::print("dirDirectionn: {}\n", static_cast<int>(getMoveDirection()));
    mBody->ApplyLinearImpulseToCenter(
        b2Vec2(mCharSettings.jumpForwardImpulse.x * getDirectionMultiplier(),
               mCharSettings.jumpForwardImpulse.y),
        true);
    */
}

void
CharacterBase::jumpUp()
{
    // TODO: physicsrelated stuff now in the PhysicsClass please move this also
    fmt::print("JumpUp x:{} y: {}\n",
               mFinalCharSettings.jumpUpImpulse.x,
               mFinalCharSettings.jumpUpImpulse.y);

    // The jump animation fires EEvent::CharacterStartJump after the
    // prepare-to-jump frames are played (currently frame 10 of IchiJumpUp);
    // only then the physics body gets its upward impulse. The sprite follows
    // the body position (see updateCharacterPosition), so the character
    // leaves the ground from this point on.
    b2Body_ApplyLinearImpulseToCenter(mBodyId,
                                      mFinalCharSettings.jumpUpImpulse,
                                      true);
}

void
CharacterBase::onNotify(const SceneNode &entity, Ryu::EEvent event)
{
    switch (event)
    {

    case Ryu::EEvent::TemporaryOutput: {
        mAnimationManager->outputStoredAnimations();
        mCharacterAnimation.setAnimationPosition(
            {mCharacterAnimation.getPosition() + positionCrossOffset});
    }

    default: {
    }
    }

    // fmt::print("Called onNotify in CharacterBase\n");
    mCharacterState->onNotify(
        *this,
        event); // notify the state (we dont want the states to be an observer)
}

sf::Shape*
CharacterBase::getShapeFromCharPhysicsBody(b2BodyId physicsBodyId) const
{
    fmt::print("BodyId: {}", physicsBodyId.index1);
    auto data = b2Body_GetUserData(physicsBodyId);
    sf::Shape* shape = reinterpret_cast<sf::RectangleShape *>(data);

    auto bodyPos = b2Body_GetPosition(physicsBodyId);
    shape->setPosition({Converter::metersToPixels(bodyPos.x),
            Converter::metersToPixels(bodyPos.y)});
    // shape->setRotation(sf::degrees(Converter::radToDeg<double>(physicsBody->GetAngle())));
    float bodyAngle = b2Rot_GetAngle(b2Body_GetRotation(physicsBodyId));
    shape->setRotation(sf::radians(bodyAngle));
    return shape;
}

void
CharacterBase::drawCurrent(sf::RenderTarget &target, sf::RenderStates) const
{
    // draw physics outline
    if (B2_IS_NON_NULL(mBodyId))
    {
        fmt::print("Drawing is delegated to the ChildClass of CharacterBase.\n");
        //target.draw(*(getShapeFromCharPhysicsBody(mBodyId)));
    }
}

CharacterBase::~CharacterBase() {
    // NOTE: the Box2D physics body is owned and destroyed by Physics
    // (Physics::~Physics destroys all bodies and the world itself).
    // Destroying it here as well would double-destroy the body and hit
    // the b2Body_IsValid assertion in Box2D, because the character can
    // outlive the Physics instance.

    // Clear animation manager (releases SFML textures)
    if (mAnimationManager) {
        mAnimationManager.reset();
    }
}

void
CharacterBase::notifyObservers(Ryu::EEvent event)
{
    notify(*this, event);
}

std::unique_ptr<CharacterState> &
CharacterBase::getCurrentCharacterState()
{
    return mCharacterState;
}

void
CharacterBase::handleInput(EInput input)
{
    // if new state is created through the input we change the mCharacterState
    // to this
    std::unique_ptr<CharacterState> state
        = mCharacterState->handleInput(*this, input);

    if (state != nullptr)
    {
        mCharacterState->exit(*this);
        mCharacterState = std::move(state);
        mCharacterState->enter(*this);
    }
}

bool
CharacterBase::allowedToFall()
{
    return (mECharacterState._value != ECharacterState::JumpUp
            && mECharacterState._value != ECharacterState::JumpForward);
}

bool
CharacterBase::inDuckMode() const
{
    return (mECharacterState._value == ECharacterState::DuckEnter
            || mECharacterState._value == ECharacterState::DuckEnd
            || mECharacterState._value == ECharacterState::DuckIdle
            || mECharacterState._value == ECharacterState::DuckWalk);
}

void
CharacterBase::setDuckState(bool duckstate)
{
    // entering/leaving the duck mode needs no body rebuild when it's
    // already in the requested mode (states call this repeatedly)
    if (mDuckStateActive == duckstate) { return; }
    mDuckStateActive = duckstate;
    if (mPhysics != nullptr)
    {
        // the body shape changes between duck/stand: rebuild it feet-anchored
        mPhysics->recreateCharacterPhysics(*this, duckstate);
    }
    else
    {
        fmt::print("WARNING: no Physics-owner set, duck body not resized\n");
    }
}

sf::Vector2f
CharacterBase::animationPositionFromBody(b2Vec2 bodyPosition) const
{
    // The duck body is shorter than the animation frame (body 34px high vs
    // 64px frames, stand body 58px): with feet-anchored bodies the duck
    // center sits (58-34)/2 = 12px lower than the stand center, so the
    // full-height frame content would sink into the ground. Shifting the
    // sprite up by that difference keeps the figure's feet on the same
    // ground line the feet-anchored body is resting on.
    const float yDuckOffset
        = inDuckMode()
              ? (INIT_FRAME_SIZE.second - DUCK_FRAME_SIZE.second) / 2.f
              : 0.f;
    return {Converter::metersToPixels<float>(bodyPosition.x),
            Converter::metersToPixels<float>(bodyPosition.y) - yDuckOffset};
}

std::string
CharacterBase::checkContactObjects()
{
    // if(contactListener.contactObject == "teleport_1")
    //{
    return contactListener.contactObject;
    //}
}

void
CharacterBase::createRaycasts()
{

    // TODO: physicsrelated stuff now in the PhysicsClass please move this also
    /*
    auto const &charPos = mCharacterAnimation.getPosition();
    mRaycastComponent.createCharacterRaycast(
        RaycastPosition::Up, charPos.x, charPos.y - Ryu::Physics::raycastOffset,
        0, mCharacterPhysicsValues.rayCastLength, getMoveDirection(),
        phWorldRef);
    mRaycastComponent.createCharacterRaycast(
        RaycastPosition::Mid, charPos.x, charPos.y, 0,
        mCharacterPhysicsValues.rayCastLength, getMoveDirection(), phWorldRef);
    mRaycastComponent.createCharacterRaycast(
        RaycastPosition::Down, charPos.x,
        charPos.y + Ryu::Physics::raycastOffset, 0,
        mCharacterPhysicsValues.rayCastLength, getMoveDirection(), phWorldRef);

    if (isFalling())
    {
        mRaycastComponent.createCharacterRaycast(
            RaycastPosition::Below, charPos.x,
            charPos.y + Ryu::Physics::raycastOffset, 90.f, 55.f,
            getMoveDirection(), phWorldRef);
    } else
    {
        mRaycastComponent.eraseBelow();
    }
    */
}

void
CharacterBase::updateCurrent(sf::Time deltaTime)
{
    //fmt::print("Updatecurrent\n");
    //update(deltaTime);
}


void
CharacterBase::update(sf::Time deltaTime)
{
    // fmt::print("UpdateCharacterBase\n");
    // std::cout //<< " x(pBody): " <<
    // Converter::metersToPixels(mBody->GetPosition().x)
    //<< " y(pBody): " << Converter::metersToPixels(mBody->GetPosition().y) <<
    //"\n"
    //          << " v " << mBody->GetLinearVelocity().x << " | " <<
    //          mBody->GetLinearVelocity().y << "\n length: " <<
    //          mBody->GetLinearVelocity().Length() << "\n";
    // TODO this has to be moved to a new state ! (falling)
    // dummy impl. / without falling animation
    // just to make sure both are set
    if (mECharacterState._value == ECharacterState::Falling)
        mCharacterFalling = true;

    /* FIXME
    ** 1. set characterFalling without physics:
    **   0: disable gravity / mass
    **   a: when physicsbody has no contact on ground -> call falling
    **   b: when character has contact on ground leave falling state
    **   c:
    ** 2. maybe when setting the cahracter without a gravity or similar the
    *flickering will go, bc. when setting the character to the ledge the
    *raycasts are set but the physics body is not bc. it falls/is stick to the
    *ground
    */

    if (b2Body_GetLinearVelocity(mBodyId).y > 0.5f)
    {
        if (allowedToFall() && not mCharacterFalling
            && mECharacterState._value != ECharacterState::Falling)
        {
            mCharacterFalling = true;
            std::unique_ptr<CharacterStateFalling> state
                = std::make_unique<CharacterStateFalling>();
            mCharacterState->exit(*this);
            mCharacterState = std::move(state);
            mCharacterState->enter(*this);
        }

        b2Vec2 bodyPosition = b2Body_GetPosition(mBodyId);

        mCharacterAnimation.setAnimationPosition(
            animationPositionFromBody(bodyPosition));
    }
    /*
        fmt::print("GravityScale: {}\n",  mBody->GetGravityScale());;
        fmt::print("Inertia: {}\n",  mBody->GetInertia());
        fmt::print("Mass: {}\n",  mBody->GetMass());
    */
    if (not inDuckMode()
        && IsInBounds(b2Body_GetLinearVelocity(mBodyId).y, 0.f, 0.01f))
    {
        mCharacterFalling = false;
    }

    createRaycasts();

    updateCharacterPosition(deltaTime);

    checkClimbingState(); // TODO: WIP
}

void
CharacterBase::checkClimbingState()
{
    // TODO: tbc
    // auto contacts = mBody->GetContactList();
}

void
CharacterBase::toggleTestStates()
{
    // TODO:
    // make this cleaner
    // as these are on another spritesheet we need to change the
    // textureatlas on the character and setup fresh preferences for
    // center/texturesize etc. / evtl. we need to change the character- position
    // (see reference point (cross in adventures when changing outfits /
    // animations))
    if (testStateCurrent == testStatesCount)
        testStateCurrent = 0;

    if (testStateCurrent == 0)
    {
        changeState(std::make_unique<CharacterStateLedgeHang>());
    }
    if (testStateCurrent == 1)
    {
        changeState(std::make_unique<CharacterStateLedgeClimbUp>());
    }

    testStateCurrent++;

    // mBody->Dump(); // what was dump() doing ?
}

void
CharacterBase::changeState(std::unique_ptr<CharacterState> toState)
{
    mCharacterState->exit(*this);
    mCharacterState = std::move(toState);
    mCharacterState->enter(*this);
}

void
CharacterBase::setupAnimation(Textures::CharacterID aniId)
{

    RyuParser::Animation aniConfig;
    auto spritesheetId = AnimationToSpritesheetID.at(aniId);
    aniConfig
        = mAnimationManager->getCharacterAnimationConfig(spritesheetId, aniId);
    fmt::print("spritesheet: {}\n", spritesheetId._to_string());
    AnimationConfiguration config{
        // frameSize.x= width / frameSize.y=height
        .frameSize = {aniConfig.frameSize.y, aniConfig.frameSize.x},
        .startFrame = {aniConfig.frames.at(0).x, aniConfig.frames.at(0).y},
        .numFrames = aniConfig.numFrames,
        .duration = aniConfig.animationDuration,
        .repeat = aniConfig.repeat,
        .animationId = aniId,
        .pivotNormalized = {aniConfig.pivot.x, aniConfig.pivot.y},
        .pivotAbsolute
        = {(int)(std::round(aniConfig.pivot.x * aniConfig.frameSize.y)),
           (int)(std::round(aniConfig.pivot.y * aniConfig.frameSize.x))}};

    fmt::print("setupAnimation for CharacterId '{}'':\n PivotNorm ({}/{})\n , "
               "PivotAbs ({}/{})\n, StartFrame ({}/{}) \n ",
               aniId._to_string(), config.pivotNormalized.x,
               config.pivotNormalized.y, config.pivotAbsolute.x,
               config.pivotAbsolute.y, aniConfig.frames.at(0).x,
               aniConfig.frames.at(0).y);

    auto &spriteAni = getSpriteAnimation();

    spriteAni.setFrameSize(config.frameSize);
    spriteAni.setStartFrame({config.startFrame.x, config.startFrame.y});
    spriteAni.setNumFrames(config.duration, aniConfig.frames);
    spriteAni.setDuration(config.duration);
    spriteAni.setRepeating(config.repeat);
    spriteAni.setAnimationName(config.animationId._to_string());
    setTextureOnCharacter(spritesheetId);

    // Set the origin from the frame size of the NEW animation (from the
    // config), not from the sprite's current texture rect: getLocalBounds()
    // still returns the PREVIOUS animation's rect here, which made the
    // origin change (and the sprite visually jump) when switching between
    // animations. With the frame size the origin only changes when a
    // spritesheet with a different frame size is set.
    spriteAni.getSprite().setOrigin(
        {static_cast<float>(config.frameSize.x) / 2.f,
         static_cast<float>(config.frameSize.y) / 2.f});
    spriteAni.setPivotAbs(config.pivotAbsolute);
    spriteAni.setPivotNorm(config.pivotNormalized);
    // the first time we need to init physics-body etc
    if (not physicsInitialized)
    {
        // initPhysics();
    }
}

/*
**
*/
void
CharacterBase::updateCharacterPosition(sf::Time deltaTime)
{
    //TODO: please check how to update the position acc to physics !!!
    mCharacterAnimation.update(deltaTime);

    //
    if (mECharacterState._value != ECharacterState::FallingEnd
        && mECharacterState._value != ECharacterState::JumpUp
        && mECharacterState._value != ECharacterState::JumpForward
        && not mCharacterFalling)
    {
        // The physics body is driven by the requested movement: the input
        // velocity is in pixels/second, Box2D works in meters. Only the
        // horizontal velocity is set, the vertical velocity stays untouched
        // so gravity and jump impulses keep working.
        b2Vec2 bodyVelocity = b2Body_GetLinearVelocity(mBodyId);
        bodyVelocity.x = Converter::pixelsToMeters<float>(movement.x);
        b2Body_SetLinearVelocity(mBodyId, bodyVelocity);

        // The animation follows the physics body: the body position is the
        // single source of truth for the character position, the sprite is
        // placed accordingly (same convention as the jump/falling code).
        b2Vec2 bodyPosition = b2Body_GetPosition(mBodyId);
        mCharacterAnimation.setAnimationPosition(
            animationPositionFromBody(bodyPosition));
    }

    if (mECharacterState._value == ECharacterState::JumpUp
        || mECharacterState._value == ECharacterState::JumpForward)
    {
        // physics body get a impulse in jump(), so here no update is needed
        auto pPosi = b2Body_GetPosition(mBodyId);
        mCharacterAnimation.setAnimationPosition(
            animationPositionFromBody(pPosi));
    }

    mCharacterState->update(*this);
}

void
CharacterBase::setMovement(sf::Vector2f _movement)
{
    movement = _movement;
}

void
CharacterBase::setMoveDirection(EMoveDirection _movementDir)
{
    mMoveDirection = _movementDir;
}

void
CharacterBase::setTexture(
    AssetManager<sf::Texture, Textures::SpritesheetID> &textureManager,
    Textures::SpritesheetID id)
{
    try{
        fmt::print("Set texture with id {} \n", (int)id);
        mCharacterAnimation.setTexture(textureManager.getResource(id));
    }
    catch(...)
    {
        fmt::print("ERROR: texture not loaded yet with id {} \n", (int)id);
    }
}

void
CharacterBase::setTextureOnCharacter(Textures::SpritesheetID textureId)
{
    // TODO implement st here ?
}

void
CharacterBase::changeColor(sf::Color color)
{
    mCharacterAnimation.getSprite().setColor(color);
}

//} /// namespace ryu
