#include <Ryu/Scene/SceneNode.h>
#include <Ryu/Core/Category.h>
#include <Ryu/Core/Command.h>

#include <SFML/Graphics/RenderStates.hpp>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <typeinfo>

//namespace ryu {
//class RenderStates;

SceneNode::SceneNode()
: mChildren()
, mParent(nullptr)
{}

SceneNode::~SceneNode() {}

void
SceneNode::attachChild(Ptr child)
{
    child->mParent = this;
    mChildren.push_back(std::move(child));
}

SceneNode::Ptr
SceneNode::detachChild(const SceneNode& node)
{
    // search element with STDL & lamda
    auto found = std::find_if(mChildren.begin(), mChildren.end(),
    [&](Ptr& p) -> bool { return p.get() == &node; });

    assert (found!=mChildren.end());

    // move foundNode in a variable
    Ptr result = std::move(*found);
    // lonewolf
    result->mParent = nullptr;
    mChildren.erase(found);
    return result;
}

void
SceneNode::draw(sf::RenderTarget& target,
                sf::RenderStates states) const
{
    // chain absolute Transform with current node relative Transform
    states.transform *= getTransform();

    // draw derived object
    drawCurrent(target, states);
    drawChildren(target, states);
}

void
SceneNode::drawChildren(sf::RenderTarget& target, sf::RenderStates states) const
{
    // draw child nodes
    for (const Ptr& child: mChildren)
    {
        child->draw(target, states);
    }
}

void
SceneNode::drawCurrent(sf::RenderTarget&, sf::RenderStates) const
{
	// Do nothing by default
}

 void 
 SceneNode::update(sf::Time dt)
 {
     updateCurrent(dt);
     updateChildren(dt);
 }

 void
 SceneNode::updateCurrent(sf::Time dt)
 {}

 void
 SceneNode::updateChildren(sf::Time dt)
 {
     for(const auto& child: mChildren)
     {
         child->update(dt);
     }
 }

 sf::Transform
 SceneNode::getWorldTransform() const
 {
     sf::Transform transform = sf::Transform::Identity;

     for(const SceneNode* node = this; node != nullptr; node = node->mParent)
     {
         transform = node->getTransform() * transform;
     }

     return transform;
 }

sf::Vector2f
SceneNode::getWorldPosition() const
{
    return getWorldTransform() * sf::Vector2f();
}

unsigned int 
SceneNode::getCategory() const
{
    return static_cast<unsigned>(Category::Type::Scene);
}

void
SceneNode::onCommand(const Command& command, sf::Time dt)
{
    // the bitwise AND operate checks if the current category and the commandscategory match the same value
    //std::cout << "rec. category: " << command.category << ", cur cat: " <<  getCategory() << "\n";
    if(command.category & getCategory())
    {
        //std::cout << "category: " << command.category << "\n";
        command.action(*this, dt);
    }

    // forward the command to all children
    for (const auto& child : mChildren)
    {
        child->onCommand(command,dt);
    }
}
//} /// namespace ryu
