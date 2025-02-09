#include <Ryu/Scene/EntityStatic.h>
#include <SFML/Graphics/Shape.hpp>
#include <memory>

//namespace ryu {

EntityStatic::EntityStatic(EntityType type) :
    mShape(nullptr)
    ,mEntityType(type)
    ,mContactPoints(0)
{}
EntityStatic::~EntityStatic() {}

void
//EntityStatic::setShape(sf::Shape* shape)
EntityStatic::setShape(std::unique_ptr<sf::Shape> shape)
{
  if(shape != nullptr) mShape = std::move(shape);
}

void
EntityStatic::increaseContactPoints()
{
  ++mContactPoints;
}

void
EntityStatic::decreaseContactPoints()
{
  --mContactPoints;
}

void
EntityStatic::updateCurrent(sf::Time dt)
{
}

void
EntityStatic::setCornerPoints(std::vector<sf::Vector2f> points)
{
  mBorderPoints = points;
}

std::vector<sf::Vector2f>
EntityStatic::getCornerPoints()
{
  return mBorderPoints;
}

std::string
EntityStatic::getName()
{
  return mName;
}

void
EntityStatic::setName(std::string name)
{
  mName = name;
}
//} /// namespace ryu
