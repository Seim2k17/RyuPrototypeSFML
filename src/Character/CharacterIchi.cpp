#include <Ryu/Character/CharacterIchi.h>
#include <Ryu/Core/Category.h>
#include <Ryu/Core/Utilities.h>
#include <Ryu/Physics/Raycast.h>
#include <Ryu/Control/CharacterEnums.h>
// test
#include <Ryu/Core/World.h>

#include <SFML/System/Vector2.hpp>
#include <box2d/b2_common.h>
#include <box2d/b2_math.h>
#include <box2d/box2d.h>
#include <SFML/Graphics.hpp>

#include <iostream>
#include <math.h>
#include <string>
//namespace ryu {

CharacterIchi::CharacterIchi(ECharacterState startState, std::unique_ptr<b2World>& phWorld,  const sf::Vector2f &position)
: CharacterBase(startState, phWorld, position)
 , ichiTextureManager()
{
    loadTextures();

    //mCharacterAnimation.setPosition({100.f,50.f});
    mCharacterState->enter(*this);
      
}

void
CharacterIchi::setTextureOnCharacter(Textures::LevelID textureId)
{
    setTexture(ichiTextureManager, textureId);
}

void
CharacterIchi::loadTextures()
{
    // At the moment we should not switch textures often on an object so we put every animatzion/level in one big spritesheet
    // and load it at the startup of the level
    // we should check what happens when we change outfits etc...
    // case level 1:
    ichiTextureManager.load(Textures::LevelID::Level1,"assets/spritesheets/ichi/ichi_spritesheet_level1.png");
    /*
    ichiTextureManager.load(Textures::CharacterID::IchiIdleRun,"assets/spritesheets/ichi/01_sheet_ichi_run.png");
    ichiTextureManager.load(Textures::CharacterID::IchiFallingLow,"assets/spritesheets/ichi/03_sheet_ichi_fall_low.png");
    */
    // Outfit combat
    //ichiTextureManager.load(Textures::CharacterID::IchiKatanaWalk,"assets/spritesheets/ichi/02_sheet_ichi_katana_walk.png");
}

unsigned int 
CharacterIchi::getCategory() const
{
    return static_cast<unsigned>(Category::Type::Player);
}

void
CharacterIchi::moveCharacter(sf::Vector2f velocity)
{
    //std::cout << "MOVE CHAR " << velocity.x << "," << velocity.y << "\n";
    /* TODO: tmp here/constran movement*/

    bool xMove = true;
    bool yMove = true;
   
    if(velocity.x == 0.f && velocity.y == 0.f)
    {
        setMovement({0.f,0.f});
    }

    // TODO: here we need to check if movement in the wanted direction is allowed
    if((xMove && velocity.x != 0.f) || (yMove && velocity.y != 0.f))
    {
        setMovement(velocity);
    }
    // std::cout << "move: " << (int)getMoveDirection() << "\n";

}

void
CharacterIchi::drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
    // t.b.c
    CharacterBase::drawCurrent(target,states);
    // draw PlayerSprite
    target.draw(mCharacterAnimation);
}

constexpr double raycastOffset = 25.0f;

/* how to use a lambda as a free function / capture ichi?
auto addRayCast = [](std::pair<double,double> points){
};
*/

void
CharacterIchi::createRaycast(std::string type, std::pair<double,double> startPoint,float angle,float length)
{
    // creating a raycast from the characters position downwards
    // 0° right / 90° up / 180° left / 270° down
    float raycastAngle = b2_pi * angle / 180.0f;
    float lengthMeter = Converter::pixelsToMeters<double>(length);
    b2Vec2 d(lengthMeter * cosf(raycastAngle),lengthMeter * sinf(raycastAngle));
    
    // direction according lookdir of character
    int8 dir = (getMoveDirection() == EMoveDirection::Right ? 1 : -1);

    b2Vec2 p1(Converter::pixelsToMeters<double>(startPoint.first),
              Converter::pixelsToMeters<double>(startPoint.second));

    b2Vec2 p2 = p1 + (dir * d); 
    
    rayCastPoints.insert(std::make_pair(type,std::make_pair(p1,p2)));
    std::cout << "RCElements:" << rayCastPoints.size() << "\n";
    
    RayCastClosest callback;
    getPhysicsWorldRef().get()->RayCast(&callback, p1,p2);
}

void 
CharacterIchi::update(sf::Time deltaTime)
{
    CharacterBase::update(deltaTime);
// TODO: visualize the debugoutput correctly, debugpoints aree not updated ...dsee world::draw)
    createRaycast("up",std::make_pair(mCharacterAnimation.getPosition().x,mCharacterAnimation.getPosition().y-raycastOffset),0,40.0f);
    createRaycast("mid",std::make_pair(mCharacterAnimation.getPosition().x,mCharacterAnimation.getPosition().y),0,40.0f);
    createRaycast("down",std::make_pair(mCharacterAnimation.getPosition().x,mCharacterAnimation.getPosition().y+raycastOffset),0,40.0f);
}

//} /// namespace ryu