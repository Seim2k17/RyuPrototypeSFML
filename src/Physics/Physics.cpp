#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Scene/EntityStatic.h"
#include "Ryu/Scene/SceneEnums.h"
#include <Ryu/Physics/Physics.h>

#include <Ryu/Core/Utilities.h>

#include <Ryu/Character/ICharacter.h>
#include <box2d/b2_body.h>
#include <box2d/b2_world.h>
#include <fmt/core.h>
#include <memory>

CharacterPhysicsParameters::CharacterPhysicsParameters() :
    mBody(nullptr),
    mFixture(nullptr),
    mGravityScale(4.8f),  /// for dynamic objects density needs to be > 0
    mFixtureDensity(5.f), /// for dynamic objects density needs to be > 0
    mFixtureFriction(0.1f), /// recommended by  b2d docu
    mFixtureRestitution(0.1f),
    mRaycastLength(40.0f),
    mMoveMultiplier({1.05f, 1.47f}),
    mJumpForwardImpulse({150, -250}),
    mJumpUpImpulse({0, -200}),
    mMassCenter({0, 0}),
    mBodyMass(25)
{}

SceneObjectPhysicsParameters::SceneObjectPhysicsParameters() :
    mPosition({}),
    mSize({}),
    mName(""),
    mType(b2BodyType::b2_staticBody),
    mTextureId(Textures::SceneID::Unknown),
    mEntityType(EntityType::None),
    mPhysicsBody(nullptr) {}
    // polygonShape.SetAsBox(0.5,0.9);


SceneObjectPhysicsParameters::SceneObjectPhysicsParameters(
        sf::Vector2i position, sf::Vector2i size,
        std::string name, b2BodyType type,
        Textures::SceneID textureId, EntityType entityType) :
    mPosition(position),
    mSize(size),
    mName(name),
    mType(type),
    mTextureId(textureId),
    mEntityType(entityType),
    mPhysicsBody(nullptr)
{}

void
Physics::initCharacterPhysics(ICharacter& character, bool inDuckMode)
 {
// TODO: CAUTION:: THERE is an thinking error and a cyclic dependency inside, TOO LATE !!!
   // TODO:
   // - initialize needed values in iniatlizer list of ctor
   // - get implementation from CharacterBase::initPhysics(...)
   // - call accordingly
   //-    // TODO: make it adjustable ? or remove and add new ? -> e.g. duck state ->
    // halfPhysics box
    // init physics after the charactersprite was created !
    // Create the body of the falling Crate
    auto position = character.getPosition();
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody; /// TODO: or even kinematic body ?
    bodyDef.position.Set(Converter::pixelsToMeters<double>(position.x),
                         Converter::pixelsToMeters<double>(
                             inDuckMode
                                 ? position.y + (DUCK_FRAME_SIZE.second / 2)
                                 : position.y));
    bodyDef.fixedRotation = true;
    auto charPhysic = mCharacterPhysics.at(character.getCharacterName());
    bodyDef.gravityScale = charPhysic.mGravityScale;

    // Create a shape
    b2PolygonShape polygonShape;

    const auto shapeSize = inDuckMode ? DUCK_FRAME_SIZE : INIT_FRAME_SIZE;

    int size_x
        = shapeSize
              .first; // mCharacterAnimation.getSprite().getTextureRect().width;
    int size_y = shapeSize.second;

    polygonShape.SetAsBox(Converter::pixelsToMeters<double>(size_x * 0.5f),
                          Converter::pixelsToMeters<double>(size_y * 0.5f));

    // Create a fixture
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &polygonShape;
    fixtureDef.density = charPhysic.mFixtureDensity;
    fixtureDef.friction = charPhysic.mFixtureFriction;
    fixtureDef.restitution = charPhysic.mFixtureRestitution;

    charPhysic.mBody = mPhysicsWorld->CreateBody(&bodyDef);
    charPhysic.mFixture = charPhysic.mBody->CreateFixture(&fixtureDef);

    // TODO: with SFML 3 there are probably smartPointer more elegant ? otherwise do RAII !
    sf::Shape *shape = new sf::RectangleShape(sf::Vector2f(size_x, size_y));
    // or can we delete this/free this at the end of the function ? bc its casted to uintptr_t
    // TODO: test it

    // std::unique_ptr<sf::Shape> shape =
    // std::make_unique<sf::RectangleShape>(sf::Vector2f(size_x, size_y));

    shape->setOrigin({size_x / 2.0f, size_y / 2.0f});
    shape->setPosition(sf::Vector2f(position.x, position.y));
    // TODO check if we need setting a texture
    // shape->setTexture(
    //    &baseTextureManager.getResource(Textures::PhysicAssetsID::Empty));

    charPhysic.mBody->GetUserData().pointer = (uintptr_t)shape; //.get();
    fmt::print("Init character at position {},{}\n",
               Converter::metersToPixels(bodyDef.position.x),
               Converter::metersToPixels(bodyDef.position.y));
    // mBody->SetLinearVelocity(b2Vec2(0.0f, -50.0f));

    // insert physics according to character
    mCharacterPhysics.insert(
        std::make_pair(character.getCharacterName(), charPhysic)
    );
}

void
Physics::createPhysicsSceneObjects(ELevel level)
{
}

void
Physics::setDebugDrawer()
{
    mPhysicsWorld->SetDebugDraw(&debugDrawer);// located as static in World
}
