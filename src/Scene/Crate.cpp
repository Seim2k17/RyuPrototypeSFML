#include <Ryu/Scene/Crate.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Physics/ShapeUtils.h>

#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <iostream>

// TODO: check how to solve this better, since SFML 3.0 the Sprite ctor needs a Texture
sf::Texture dummyTexture = sf::Texture();

Crate::Crate()
: Sprite(dummyTexture)
, mBodyId()
, mCrateTextureManager()
{
    // Outfit normal
    mCrateTextureManager.load(Textures::PhysicAssetsID::Crate,"assets/scenes/99_dummy/box_wood.png");
}

Crate::~Crate(){}

void
Crate::init(b2BodyId pBody, sf::Shape* pShape)
{
    mBodyId = pBody;
    mShape = pShape;
}

void
Crate::init(b2WorldId phWorld, const glm::vec2 &position, const glm::vec2 &dimension)
{
    // Create the body of the falling Crate
    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody;
    bodyDef.position = (b2Vec2){position.x,position.y};
    mBodyId = b2CreateBody(phWorld, &bodyDef);

    // Create the shape (v3.1); clamped so a non-positive dimension cannot
    // trip B2_ASSERT(half > 0) in b2MakeBox
    b2Polygon box = RyuPhysics::makeSafeBox(dimension.x, dimension.y, "crate");

    b2SurfaceMaterial material =b2DefaultSurfaceMaterial();
    material.friction = 0.3f; /// recommended by b2d docu
    
    b2ShapeDef shapeDef = b2DefaultShapeDef();
    shapeDef.density = 1.f; /// for dynamic objects density needs to be > 0
    shapeDef.material = material;  

    // TODO: guess I need to store the ID somewhere !
    b2ShapeId shapeId = b2CreatePolygonShape(mBodyId, &shapeDef, &box);
}
