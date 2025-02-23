#include "Ryu/Animation/SpritesheetAnimation.h"
#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Statemachine/CharacterStateRun.h"
#include <Ryu/Character/CharacterIchi.h>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Core/Category.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Physics/Raycast.h>
#include <Ryu/Physics/RaycastTypes.h>
// test
#include <Ryu/Core/World.h>

#include <SFML/Graphics.hpp>
#include <SFML/System/Vector2.hpp>
#include <box2d/b2_common.h>
#include <box2d/b2_math.h>
#include <box2d/box2d.h>
#include <fmt/core.h>

#include <iostream>
#include <math.h>
#include <string>
// namespace ryu {
class CharacterStateRun;

CharacterIchi::CharacterIchi(ECharacterState startState,
                             const sf::Vector2i &position)
    : CharacterBase(startState, position), ichiTextureManager()
{
    loadTextures();
    // mCharacterAnimation.setPosition({100.f,50.f});
    mCharacterState->enter(*this);
}

void
CharacterIchi::setTextureOnCharacter(Textures::SpritesheetID textureId)
{
    setTexture(ichiTextureManager, textureId);
}

void
CharacterIchi::teleportCharacter(float x, float y)
{
    b2Vec2 newBodyPos{Converter::pixelsToMeters<float>(x),
                      Converter::pixelsToMeters<float>(y)};
    getBody()->SetTransform(newBodyPos, 0);
    changeState(std::make_unique<CharacterStateRun>());
    auto pPosi = getBody()->GetPosition();
    getSpriteAnimation().setAnimationPosition(
        {Converter::metersToPixels(pPosi.x),
         Converter::metersToPixels(pPosi.y)});
    moveCharacter(
        sf::Vector2f{1, 0}); // the trick is to push the character a bit
}

void
CharacterIchi::onNotify(const SceneNode &entity, Ryu::EEvent event)
{
    CharacterBase::onNotify(*this, event);
    fmt::print("Called onNotify in CharacterIchi: Event: {}\n",
               event._to_string());
}

// TODO: for the start ths will make it but its probably better to
// 1. put this into the animationmanager
// 2. automatize this by loading the spritesheets when the config files are
// loaded at startup
// 3. also move  the texturemamager to the animationmanager ?
static std::map<Textures::SpritesheetID, std::string> spritesheePaths = {
    {Textures::SpritesheetID::Ichi80x96,
     //"assets/spritesheets/ichi/ichi_spritesheet_level1.png"},
     "assets/spritesheets/ichi/ichi_level1.png"},
    /*
        {Textures::SpritesheetID::Ichi128x196,
        "assets/spritesheets/ichi/ichi_climb_up_ledge_128x196.png"},*/
};

void
CharacterIchi::loadTextures()
{
    // At the moment we should not switch textures often on an object so we put
    // every animatzion/level in one big spritesheet and load it at the startup
    // of the level we should check what happens when we change outfits etc...
    for (const auto &id : spritesheePaths) {
        ichiTextureManager.load(id.first, id.second);
    }
    // Outfit combat
    // ichiTextureManager.load(Textures::CharacterID::IchiKatanaWalk,"assets/spritesheets/ichi/02_sheet_ichi_katana_walk.png");
}

unsigned int
CharacterIchi::getCategory() const
{
    return static_cast<unsigned>(Category::Type::Player);
}

void
CharacterIchi::allowedMovement(bool &xMove, bool &yMove)
{

    if ((mECharacterState._value == ECharacterState::DuckIdle
         || mECharacterState._value == ECharacterState::DuckEnter
         || mECharacterState._value == ECharacterState::DuckEnd
         || mECharacterState._value == ECharacterState::DuckWalk)
        || mECharacterMovement != ECharacterMovement::CanClimb) {
        yMove = false;
    }

    /*
        if(mECharacterState._value == ECharacterState::JumpUp)
        {
            yMove = true;
        }
    */
}

void
CharacterIchi::moveCharacter(sf::Vector2f velocity)
{
    // std::cout << "MOVE CHAR " << velocity.x << "," << velocity.y << "\n";
    /* TODO: tmp here/constran movement*/

    bool xMove = true;
    bool yMove = true;

    allowedMovement(xMove, yMove);

    if (mECharacterState._value != ECharacterState::JumpUp && velocity.x == 0.f
        && velocity.y == 0.f) {
        setMovement({0.f, 0.f});
    }

    // TODO: here we need to check if movement in the wanted direction is
    // allowed
    if ((xMove && velocity.x != 0.f) || (yMove && velocity.y != 0.f)) {
        setMovement(velocity);
    }
    // std::cout << "move: " << (int)getMoveDirection() << "\n";
}

void
CharacterIchi::drawCurrent(sf::RenderTarget &target,
                           sf::RenderStates states) const
{
    // draw physicsshape (visible in debug-mode)
    CharacterBase::drawCurrent(target, states);
    // draw PlayerSprite
    target.draw(mCharacterAnimation);
}

void
CharacterIchi::checkTeleportContact(std::string name)
{
    if (name == "teleport_1") {
        fmt::print("Now we teleport in Ichi to posi 1\n");
        teleportCharacter(50, 20);
    }
    if (name == "teleport_2") {
        fmt::print("Now we teleport in Ichi to posi 1\n");
        teleportCharacter(1000, 270);
    }
}

// TODO: check here flip st to do with raycasts?
void
CharacterIchi::update(sf::Time deltaTime)
{

    CharacterBase::update(deltaTime);

    checkTeleportContact(CharacterBase::checkContactObjects());

    /*
        if(rayCastCallbacks.at("up").m_Hit) fmt::print("Up: RayCastHit at b2D
       {}/{} and \n sfml {}/{}\n", rayCastCallbacks.at("up").m_Point.x
       ,rayCastCallbacks.at("up").m_Point.y
                     ,Converter::metersToPixels<double>(rayCastCallbacks.at("up").m_Point.x),
       Converter::metersToPixels<double>(rayCastCallbacks.at("up").m_Point.y));
    */
}

//} /// namespace ryu
