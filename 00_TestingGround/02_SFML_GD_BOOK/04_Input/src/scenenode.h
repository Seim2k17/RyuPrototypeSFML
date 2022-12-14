#pragma once

#include "command.h"

#include <SFML/Graphics.hpp>
#include <memory>
#include <vector>


class SceneNode : public sf::Drawable, 
                public sf::Transformable, 
                private sf::NonCopyable
{
    public:
        typedef std::unique_ptr<SceneNode> Ptr;

    public:
        SceneNode();

        void attachChild(Ptr child);
        Ptr detachChild(const SceneNode& node);
        void update(sf::Time dt);
        virtual unsigned int getCategory() const;
        sf::Transform getWorldTransform() const;
        sf::Vector2f getWorldPosition() const;
        void onCommand(const Command& command, sf::Time dt);
    
    private:
        void draw(sf::RenderTarget& target, sf::RenderStates states) const final;
        virtual void drawCurrent(sf::RenderTarget& target, sf::RenderStates states) const;
        virtual void updateCurrent(sf::Time dt);
        void updateChildren(sf::Time dt);

    private:
        std::vector<Ptr> mChildren;
        SceneNode* mParent;
};
