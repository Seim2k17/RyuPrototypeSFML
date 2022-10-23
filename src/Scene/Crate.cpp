#include <Ryu/Scene/Crate.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/AssetIdentifiers.h>

#include <iostream>


Crate::Crate()
: mBody(nullptr)
, mFixture(nullptr)
, mCrateTextureManager()
{
    // Outfit normal
    mCrateTextureManager.load(Textures::PhysicAssetsID::Crate,"assets/scenes/99_dummy/box_wood.png");
}

Crate::~Crate(){}

void
Crate::init(b2World* phWorld, const glm::vec2 &position, const glm::vec2 &dimension)
{
    // Create the body of the falling Crate
    b2BodyDef bodyDef;
    bodyDef.type = b2_dynamicBody;
    bodyDef.position.Set(position.x,position.y);

    mBody = phWorld->CreateBody(&bodyDef);

    // Create a shape
    b2PolygonShape polygonShape;
    polygonShape.SetAsBox(dimension.x/2.f,dimension.y/2.f);

    // Create a fixture
    b2FixtureDef fixtureDef;
    fixtureDef.shape = &polygonShape;
    fixtureDef.density = 1.f; /// for dynamic objects density needs to be > 0
    fixtureDef.friction = 0.3f; /// recommended by b2d docu 
    mFixture = mBody->CreateFixture(&fixtureDef);
    std::cout << "Init crate at position "<< bodyDef.position.x << ","<< bodyDef.position.y << "\n";

}