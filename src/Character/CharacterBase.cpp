#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Statemachine/CharacterStateIdle.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/Utilities.h>

#include <box2d/box2d.h>
#include <Thirdparty/glm/glm.hpp>

#include <iostream>
#include <memory>
#include <algorithm>

//namespace ryu {

// this is the framesize for the boundary box of the physics body
static constexpr std::pair<int,int> INIT_FRAME_SIZE(60,86); 
 
template <typename T>
    bool IsInBounds(const T& value, const T& low, const T& high) {
    return !(value < low) && !(high < value);
}


CharacterBase::CharacterBase(std::unique_ptr<b2World>& phWorld,  
                            const sf::Vector2f &position) 
    :mCharacterAnimation()
    ,mCharacterState(std::make_unique<CharacterStateIdle>())
    ,movement(0.f,0.f)
    ,mMoveDirection(EMoveDirecton::Right)
    ,phWorldRef(phWorld)
    ,mCharacterFalling(false)
    ,baseTextureManager()
    ,mCharSettings()
    ,mDebugDraw(false)
{
    mDebugDraw = mCharSettings.DebugDraw;
    loadTextures();
}


CharacterBase::CharacterBase(ECharacterState startState, 
                            std::unique_ptr<b2World>& phWorld,  
                            const sf::Vector2f &position)
    :mECharacterState(startState)
    ,mCharacterState(std::make_unique<CharacterStateIdle>())
    ,mCharacterSpeed(55.0f) // startvalue playerspeed
    ,mMoveDirection(EMoveDirecton::Right)
    ,phWorldRef(phWorld)
    ,mCharacterFalling(false)
    ,baseTextureManager()
    ,mCharSettings()
    ,mDebugDraw(false)
{
   // needable ? 
   handleInput(EInput::ReleaseLeft);
  
   mDebugDraw = mCharSettings.DebugDraw;
   loadTextures();

   switch(mECharacterState)
   {
       case ECharacterState::Idle:
        mCharacterState = std::make_unique<CharacterStateIdle>();
        break;
       default:
        mCharacterState = std::make_unique<CharacterStateIdle>();
   }
}

void
CharacterBase::updatePhysics()
{
    sf::Vector2f position = mCharacterAnimation.getPosition();
    
    //initPhysics(phWorldRef,position);
}

void
CharacterBase::updatePhysics(const sf::Vector2f &position)
{
    //initPhysics(phWorldRef,position);
}

// inits the physics at the current character position, used afte the initial state is set 
void
CharacterBase::initPhysics()
{
    if(not physicsInitialized)
    {
        initPhysics(phWorldRef, mCharacterAnimation.getPosition());
        physicsInitialized = true;
    }   
}

void
CharacterBase::loadTextures()
{
    baseTextureManager.load(Textures::PhysicAssetsID::Empty,"assets/scenes/99_dummy/box_empty.png");
}

void
CharacterBase::initPhysics(std::unique_ptr<b2World>& phWorld,  const sf::Vector2f &position)
{
    // init physics after the charactersprite was created !
    // Create the body of the falling Crate
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody; /// TODO: or even kinematic body ?
    bodyDef.position.Set(Converter::pixelsToMeters<double>(position.x),
                         Converter::pixelsToMeters<double>(position.y));
    bodyDef.fixedRotation = true;                     
    bodyDef.gravityScale = 4.8f;

    // Create a shape
    b2PolygonShape polygonShape;
    // TODO write convert functions Pixels<->meter (box2d) and reset polygonshape wenn aniation changes
    // polygonShape.SetAsBox(mCharacterAnimation.getTexture()->getSize().x / 20.f, mCharacterAnimation.getTexture()->getSize().y / 20.f ); /* dimension.x/2.f,dimension.y/2.f */
    //polygonShape.SetAsBox(0.5,0.9);

    int size_x = INIT_FRAME_SIZE.first;// mCharacterAnimation.getSprite().getTextureRect().width;
    int size_y = INIT_FRAME_SIZE.second;// mCharacterAnimation.getSprite().getTextureRect().height;

    polygonShape.SetAsBox(
         Converter::pixelsToMeters<double>(size_x * 0.5f)
        ,Converter::pixelsToMeters<double>(size_y * 0.5f));

    // Create a fixture
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &polygonShape;
    fixtureDef.density = 5.f; /// for dynamic objects density needs to be > 0
    fixtureDef.friction = 0.1f; /// recommended by b2d docu 
    fixtureDef.restitution = 0.1;

    mBody = phWorld->CreateBody(&bodyDef);
    mFixture = mBody->CreateFixture(&fixtureDef);
    
    sf::Shape* shape = new sf::RectangleShape(sf::Vector2f(size_x,size_y));
    shape->setOrigin(size_x/2.0f,size_y/2.0f);                                                                                                                                              
    shape->setPosition(sf::Vector2f(position.x,position.y));
    // TODO make bitsize style like category selection !!!
    if(mDebugDraw)
    {
        shape->setFillColor(sf::Color::Magenta);
    }
    else
    {
        shape->setTexture(&baseTextureManager.getResource(Textures::PhysicAssetsID::Empty));    
    
    }
    
    mBody->GetUserData().pointer = (uintptr_t)shape;

    std::cout << "Init character at position "<< Converter::metersToPixels(bodyDef.position.x) << "," << Converter::metersToPixels(bodyDef.position.y) << "\n";
    //mBody->SetLinearVelocity(b2Vec2(0.0f, -50.0f));
}

sf::Shape*
CharacterBase::getShapeFromCharPhysicsBody(b2Body* physicsBody) const
{
    b2BodyUserData& data = physicsBody->GetUserData();
    sf::Shape* shape = reinterpret_cast<sf::RectangleShape*>(data.pointer);

    shape->setPosition(Converter::metersToPixels(physicsBody->GetPosition().x),
                       Converter::metersToPixels(physicsBody->GetPosition().y));
    shape->setRotation(Converter::radToDeg<double>(physicsBody->GetAngle()));
    return shape;
}

void
CharacterBase::drawCurrent(sf::RenderTarget& target, sf::RenderStates) const
{
    // t.b.c
      if(mBody)
    {
        target.draw(*(getShapeFromCharPhysicsBody(mBody)));
    }
    
}

CharacterBase::~CharacterBase() {}

void 
CharacterBase::notifyObservers(Event event)
{   
    notify(*this,event);
}

std::unique_ptr<CharacterState>& 
CharacterBase::getCurrentCharacterState()
{
    return mCharacterState;
}

void 
CharacterBase::handleInput(EInput input)
{
    // if new state is created through the input we change the mCharacterState to this
    std::unique_ptr<CharacterState> state = mCharacterState->handleInput(*this,input);

    if(state != nullptr)
    {
        mCharacterState->exit(*this);
        mCharacterState = std::move(state);
        mCharacterState->enter(*this);
    }
}
 
void
CharacterBase::update(sf::Time deltaTime)
{
    std::cout //<< " x(pBody): " << Converter::metersToPixels(mBody->GetPosition().x)
              //<< " y(pBody): " << Converter::metersToPixels(mBody->GetPosition().y) << "\n"
              << " v " << mBody->GetLinearVelocity().x << " | " << mBody->GetLinearVelocity().y << "\n length: " << mBody->GetLinearVelocity().Length() << "\n";
    // TODO this has to be moved to a new state ! (falling)
    // dummy impl. / without falling animation
    if(mBody->GetLinearVelocity().y > 0 )
    {   mCharacterFalling = true;     
        mCharacterAnimation.setPosition(
            Converter::metersToPixels<double>(mBody->GetPosition().x),
            Converter::metersToPixels<double>(mBody->GetPosition().y)
        );
    }

    if(IsInBounds(mBody->GetLinearVelocity().y,0.f, 0.02f))
    {   
        mCharacterFalling = false;     
    }

    updateCharacterState(deltaTime);

}

void
CharacterBase::updateCharacterState(sf::Time deltaTime)
{
    mCharacterAnimation.update(deltaTime);
    mCharacterAnimation.move(movement * deltaTime.asSeconds());
    if(not mCharacterFalling)
    {
        mBody->SetLinearVelocity( {mCharSettings.MoveMultiplierX * Converter::pixelsToMeters<float>(movement.x),
                                   mCharSettings.MoveMultiplierY * Converter::pixelsToMeters<float>(movement.y)});
    }
    
    mCharacterState->update(*this);
}

 void 
 CharacterBase::setMovement(sf::Vector2f _movement)
 {
     movement = _movement;
 }

 void 
 CharacterBase::setMoveDirection(EMoveDirecton _movementDir)
 {
     mMoveDirection = _movementDir;
 }

void 
CharacterBase::setTexture(AssetManager<sf::Texture, Textures::CharacterID> &textureManager, Textures::CharacterID id)
{
    mCharacterAnimation.setTexture(textureManager.getResource(id));
}

void
CharacterBase::setTextureOnCharacter(Textures::CharacterID textureId)
{
    // TODO implement st here ?
    
}

void
CharacterBase::changeColor(sf::Color color)
{
    //mCharacterAnimationShape.setFillColor(color);
}

//} /// namespace ryu
