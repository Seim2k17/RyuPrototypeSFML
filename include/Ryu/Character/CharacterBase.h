#pragma once

// #include <Ryu/Animation/Animation.h>
#include "Ryu/Animation/AnimationManager.h"
#include "Ryu/Control/PlayerController.h"
#include "Ryu/Debug/b2DrawSFML.hpp"
//#include "Ryu/Physics/RaycastTypes.h"
#include <Ryu/Animation/SpritesheetAnimation.h>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Character/ICharacter.h>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Events/Observer.h>
#include <Ryu/Events/Subject.h>
#include <Ryu/Physics/RaycastComponent.h>
#include <Ryu/Scene/ContactListener.h>
#include <Ryu/Scene/EntityStatic.h>
#include <Ryu/Scene/SceneNode.h>
#include <Ryu/Statemachine/CharacterState.h>

#include <SFML/Graphics.hpp>
#include <Thirdparty/glm/glm.hpp>
#include <box2d/b2_contact.h>
#include <box2d/b2_math.h>
#include <box2d/box2d.h>
#include <fmt/core.h>
#include <iostream>
#include <memory.h>
#include <memory>
#include <vector>

using BaseTextureManager = AssetManager<sf::Texture, Textures::PhysicAssetsID>;

namespace sf {
class Event;
}

class CommandQueue;
class b2World;
class b2Body;
class b2Vec2;
class b2Fixture;
// namespace ryu{

struct CharacterSetting {
};

struct CharacterFinalSetting {
    // due increase of gravityscale (falling is then more gamey) the physicbody
    // needs some adustments for movement so its not behind the movement of
    // the characteranimation
    float MoveMultiplierX;
    float MoveMultiplierY;

    b2Vec2 jumpForwardImpulse;
    b2Vec2 jumpUpImpulse;
    b2Vec2 massCenter;
    float bodyMass;

    CharacterFinalSetting() {
        MoveMultiplierX = 1.05f;
        MoveMultiplierY = 1.47f;

        jumpForwardImpulse = {150, -250};
        jumpUpImpulse = {0, -200};
        massCenter = {0, 0};
        bodyMass = 25;
    }
};

struct AnimationConfiguration {
    sf::Vector2i frameSize;
    sf::Vector2i startFrame;
    std::size_t numFrames;
    sf::Time duration;
    bool repeat;
    Textures::CharacterID animationId;
    sf::Vector2f pivotNormalized;
    sf::Vector2i pivotAbsolute;
};

struct CharacterPhysicsValues {

    // TODO: not directly physics but physicsrelated. so keep it in CharacterBaseClass
    std::pair<int, int> getFrameSize(bool characterDuck) {
        std::pair<int, int> size;
        if (characterDuck) {
            size.first =
                DUCK_FRAME_SIZE
                    .first; // mCharacterAnimation.getSprite().getTextureRect().width;
            size.second =
                DUCK_FRAME_SIZE
                    .second; // mCharacterAnimation.getSprite().getTextureRect().height;
        } else {
            size.first =
                INIT_FRAME_SIZE
                    .first; // mCharacterAnimation.getSprite().getTextureRect().width;
            size.second =
                INIT_FRAME_SIZE
                    .second; // mCharacterAnimation.getSprite().getTextureRect().height;
        }
        return size;
    }
};

static std::map<Textures::CharacterID, Textures::SpritesheetID>
    AnimationToSpritesheetID = {
        {Textures::CharacterID::IchiDuckEnter,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiDuckIdle,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiDuckWalk,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiFallingLoop,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiIdle, Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiJumpForwardNrml,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiJumpUp, Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLadderBack,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLadderClimbLoop,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLadderEnter,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLandHigh,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLadderIdle,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLandLow,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiRun, Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiWalk, Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiStartFalling,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiSword1Idle,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiSword1Walk,
         Textures::SpritesheetID::Ichi80x96},
        {Textures::CharacterID::IchiLedgeClimbUp,
         Textures::SpritesheetID::Ichi80x96},
        // TODO: redo spritesheet process with the following animation !
        {Textures::CharacterID::IchiLedgeHangIdle,
         Textures::SpritesheetID::Ichi80x96},
};

class AnimationManager;

class CharacterBase : public SceneNode, public Subject, public Observer, public ICharacter {

  public:
    // TODO: implement rule of 5 !
    // (morph one character into another ^^)
    CharacterBase(const sf::Vector2i &position);
    CharacterBase(ECharacterState startState,
                  const sf::Vector2i &position);
    ~CharacterBase();

    float getCharacterSpeed() { return mCharacterSpeed; }
    void setCharacterSpeed(float speed) { mCharacterSpeed = speed; };
    void setPhysicWorldRef(std::unique_ptr<b2World> &phWorld);

    void setupAnimation(Textures::CharacterID aniId);

    std::unique_ptr<CharacterState> &getCurrentCharacterState();

    virtual void setTextureOnCharacter(Textures::SpritesheetID textureId);
    virtual void setTexture(
        AssetManager<sf::Texture, Textures::SpritesheetID> &textureManager,
        Textures::SpritesheetID id);
    SpritesheetAnimation &getSpriteAnimation() { return mCharacterAnimation; }

    void setMovement(sf::Vector2f _movement);
    void setMoveDirection(EMoveDirection _movementDir);
    EMoveDirection getMoveDirection() { return mMoveDirection; }

    // TODO: check how to move to Physics-class
/*
    void initPhysics();
    void destroyPhysics();
    void updatePhysics();
    void updatePhysics(const sf::Vector2f &position);
*/
    void checkClimbingState();
    std::string checkContactObjects();

    virtual void handleInput(EInput input);
    virtual void update(sf::Time deltaTime);
    void updateCharacterPosition(sf::Time deltaTime);
    virtual void loadTextures();
    void changeState(std::unique_ptr<CharacterState> toState);
    // void setupAnimation(AnimationConfiguration config);

    void changeColor(sf::Color color);

    void notifyObservers(Ryu::EEvent event);
    bool isFalling() { return mCharacterFalling; }
    void setFalling(bool falling) { mCharacterFalling = falling; }

    b2Body *getBody() { return mBody; }
    b2Fixture *getFixture() { return mFixture; }
    ECharacterState getCharacterStateEnum() { return mECharacterState; }
    EActionHeight getActionHeight() { return mActionHeight; }
    void setCharacterStateEnum(ECharacterState stateValue) {
        mECharacterState = stateValue;
    }
    void setActionHeight(EActionHeight heightValue) {
        mActionHeight = heightValue;
    }

    /*** Raycastaccess
     * \brief Raycasts are used to determine ledges for climbing
     *  or the ground when in falling state to play a
     * "prepare-to-land-animation". The implementation-details are in the
     * raycastcomponent.
     */
    bool getHit(RaycastPosition rcName) {
        return mRaycastComponent.getHit(rcName);
    }
    void createRaycasts();
    void eraseRaycast(RaycastPosition rcName) {
        mRaycastComponent.eraseRaycast(rcName);
    }
    void eraseRaycastPoints(RaycastPosition rcName) {
        mRaycastComponent.eraseRaycastPoints(rcName);
    }
    void drawRaycasts(b2DrawSFML &debugDrawer) {
        mRaycastComponent.drawRaycasts(debugDrawer);
    }
    // \ Raycastaccess

    b2Vec2 getLinearVelocity() { return mBody->GetLinearVelocity(); }
    bool allowedToFall();
    bool inDuckMode();
    void jumpUp();
    void jumpForward();
    float getDirectionMultiplier();
    [[deprecated]] void setCharacterSettings(CharacterSetting settings);
    [[deprecated]] void resetCharacterSettings();
    bool duckStateActive() { return mDuckStateActive; };
    void setDuckState(bool duckstate) { mDuckStateActive = duckstate; };
    virtual void onNotify(const SceneNode &entity, Ryu::EEvent event) override;
    void ouputAnimations() { mAnimationManager->outputStoredAnimations(); }
    void toggleTestStates();
    void setPositionOffset(sf::Vector2f offset);
    void setOffset(bool state) { mSetOffset = state; };
    bool getOffsetState() { return mSetOffset; };
    sf::Vector2f getPositionCross() { return positionCrossOffset; }
    sf::Vector2f getPosition() override { return mCharacterAnimation.getPosition(); }
        ECharacters getCharacterName() override { return ECharacters::Ichi;}

  protected:
    /***
     * \brief   Initialized physic (body, fixtures for the character).
     *          its important to call the method in the child class,
     *          because just there the animation sprite is set and the size &
     *mass could be calculated
     *
     ***/
    // TODO: move the physics section completetly to the physicsclass !
    void initPhysics(std::unique_ptr<b2World> &phWorld,
                     const sf::Vector2f &position); // TODO: what for ?
    // TODO: rename to 'drawPhysicsOutline'
    void drawCurrent(sf::RenderTarget &target,
                     sf::RenderStates states) const override;

    sf::Shape *getShapeFromCharPhysicsBody(b2Body *physicsBody) const;

    SpritesheetAnimation mCharacterAnimation;

    std::unique_ptr<CharacterState> mCharacterState;
    ECharacterState mECharacterState;
    float mCharacterSpeed;
    bool physicsInitialized;
    Ryu::Physics::RaycastComponent mRaycastComponent;

  private:
    BaseTextureManager baseTextureManager;
    bool mDuckStateActive;
    int testStatesCount = 2;
    int testStateCurrent = 0;
    sf::Vector2f positionCrossOffset{0, 0};
    sf::Vector2f lastPositionCrossOffset{0, 0};
    bool mSetOffset;
    b2Vec2 mLastBodyPosition;
    RyuContactListener contactListener;

  protected:
    std::unique_ptr<AnimationManager> mAnimationManager;
    EMoveDirection mMoveDirection;
    EActionHeight mActionHeight;

    sf::Vector2f movement;
    bool mCharacterFalling;

    // TODO: physics -> physics interface
    //std::unique_ptr<b2World> &phWorldRef;
    b2Body *mBody;
    b2Fixture *mFixture;
    Textures::LevelID mCurrentLevel;
    CharacterPhysicsValues mCharacterPhysicsValues;

    ECharacterMovement mECharacterMovement;

  public:
    CharacterSetting mCharSettings{};
    CharacterFinalSetting mFinalCharSettings{};
};

//} /// namespace ryu
