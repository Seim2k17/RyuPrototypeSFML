#include <Ryu/Scene/Box.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Core/AssetManager.h>
#include <SFML/Graphics/Transformable.hpp>
#include <iostream>
#include <Ryu/Core/Category.h>

//namespace ryu {

Textures::SceneID toTextureID(Box::Type type)
{
    switch (type)
    {
        case Box::Type::Pushable:
            return Textures::SceneID::BoxPushable;
        case Box::Type::Heavy:
            return Textures::SceneID::BoxHeavy;
        default:
            return Textures::SceneID::Unknown;
    }
}

Box::Box(Type boxType, const SceneTextureHolder& textures) 
    : mType(boxType), mSprite(textures.getResource(toTextureID(boxType)))

{
    setPosition({760.f,60.f});
    // TODO: add another ctor for positioning !
    
    // set origin of texture to center 
    sf::FloatRect bounds = mSprite.getLocalBounds();
    mSprite.setOrigin(bounds.getCenter()); //er() width / 2.f, bounds.height / 2.f);
    setVelocity(sf::Vector2f(50.f,0.f));
}


void
Box::updateCurrent(sf::Time dt)
{
    sf::Vector2f position = getPosition();
    sf::Vector2f velocity = getVelocity();

    // TODO: no magic mnumbers
    if ((position.x <= 750)
    || (position.x >= 1000))
    {
        velocity.x = -velocity.x;
        setVelocity(velocity); 
    }

    move({velocity.x * dt.asSeconds(),0.f});

    // t,b,c
}



void 
Box::drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const
{
    target.draw(mSprite, states);
}

unsigned int 
Box::getCategory() const
{
    switch (mType)
    {
        case Box::Type::Heavy:
        {
            return static_cast<unsigned>(Category::Type::StaticObjects);
        }
        case Box::Type::Pushable:
        {
            return static_cast<unsigned>(Category::Type::MovableObjects);
        }
       
    default:
            return static_cast<unsigned>(Category::Type::StaticObjects);
   } 
} 

//} /// namespace ryu
