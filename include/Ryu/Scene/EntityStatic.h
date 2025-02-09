#pragma once

//#include <Ryu/Scene/SceneNode.h>
#include <Ryu/Core/Category.h>
#include <SFML/Graphics.hpp>
#include <fmt/core.h>
#include <memory>
#include <vector>

enum class EntityType {
    None = 0,
    Climbable = 1,
};

//namespace ryu {
class EntityStatic //: public SceneNode
{
    public:
        // t.b.c
        EntityStatic(EntityType type);
        virtual ~EntityStatic();
        unsigned int getCategory() const { return static_cast<unsigned>(Category::Type::None);}
        EntityType getEntityType() {return mEntityType;};

        [[deprecated]]
        void setShape(sf::Shape* shape);
        void setShape(std::unique_ptr<sf::Shape> shape);

        sf::Shape* getShape(){
                return mShape.get();
        };
        void setCornerPoints(std::vector<sf::Vector2f> points);
        std::vector<sf::Vector2f> getCornerPoints();
        std::string getName();
        void setName(std::string name);
        void increaseContactPoints();
        void decreaseContactPoints();
        uint16_t getContactPoints() {return mContactPoints;};

private:
        uint16_t mContactPoints;
        virtual void updateCurrent(sf::Time dt);
        std::unique_ptr<sf::Shape> mShape;
        EntityType mEntityType;
        std::vector<sf::Vector2f> mBorderPoints;
        std::string mName;


};
//} /// namespace ryu_
