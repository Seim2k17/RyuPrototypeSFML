/**
 * This class is responsible for all physicsrelated stuff for Characters and for SceneObjects
 * @author: Simon Nguyen
 * @date: 02/02/2025
 *
 * */

#pragma once

#include "Ryu/Character/CharacterBase.h"
#include "Ryu/Debug/b2DrawSFML.hpp"
#include "Ryu/Events/Subject.h"
#include "Ryu/Scene/EntityStatic.h"
#include "Ryu/Scene/SceneEnums.h"
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Control/CharacterEnums.h>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <memory>
#include <map>

class CharacterBase;
class ICharacter;
class RenderWindow;
class Shape;

static b2DrawSFML debugDrawer;

struct CharacterPhysicsParameters{
    CharacterPhysicsParameters();

    b2Body* mBody;
    b2Fixture* mFixture;
    float mGravityScale;
    float mFixtureDensity;
    float mFixtureFriction;
    float mFixtureRestitution;
    float mRaycastLength;
    // due increase of gravityscale (falling is then more gamey) the physicbody
    // needs some adustments for movement so its not behind the movement of
    // the characteranimation
    sf::Vector2f mMoveMultiplier;

    b2Vec2 mJumpForwardImpulse;
    b2Vec2 mJumpUpImpulse;
    b2Vec2 mMassCenter;
    float mBodyMass;
};

struct SceneObjectPhysicsParameters
{
    SceneObjectPhysicsParameters();
    SceneObjectPhysicsParameters(
        sf::Vector2i position, sf::Vector2i size,
        std::string name, b2BodyType type = b2_staticBody,
        Textures::SceneID textureId = Textures::SceneID::Grass, EntityType entityType=EntityType::None);

    sf::Vector2i mPosition;
    sf::Vector2i mSize;
    std::string mName;
    b2BodyType mType;
    Textures::SceneID mTextureId;
    EntityType mEntityType;
    b2Body* mPhysicsBody;
};


class Physics : public Subject {
   public:
    Physics();
    ~Physics();

    void createPhysicsSceneObjects(ELevel level);
    void debugDrawSegment(const b2Vec2& p1, const b2Vec2& p2,
                          const  b2Color& color) const;
    void debugDraw() const;
    void draw(sf::RenderWindow& window);
    sf::Shape* getShapeFromPhysicsBody(b2Body* physicsBody);
    void initCharacterPhysics(ICharacter& character, bool inDuckMode);
    void setDebugDrawer(b2DrawSFML dbgDrawer);
    void setDebugPhysics(bool debugPhysics);
    void update();

   private:
    void createPhysicsBody(SceneObjectPhysicsParameters& sceneObject);
    // box2d physics
    std::map<ECharacters, CharacterPhysicsParameters> mCharacterPhysics;
    std::unique_ptr<b2World> mPhysicsWorld;
    //b2World* mPhysicsWorld;
    // std::map<ELevel, std::vector<SceneObjectPhysicsParameters> > mSceneObjects; // see sceneObjects

    // TODO: check if we keep it here or not ? Do we even need this at all ?                                                                   // key is a combination of LevelName (ELevel.string) and ObjectName ? WHY ?
    // it was: why?
    std::map<const uintptr_t, std::unique_ptr<EntityStatic> > mStaticEntities;
    // std::map<uintptr_t, EntityStatic > mStaticEntities;
    float mPhTimeStep;
    bool mDebugPhysicsActive;

   public: // TODO: tmp storage !!!! -> make static again outside of class
    std::map<ELevel , std::vector<SceneObjectPhysicsParameters>> sceneObjects = {
    {ELevel::Level1,
     {{{600, 780}, {1200, 20}, "floor"},
      {{8, 580}, {16, 800}, "left_side"},
      {{1190, 580}, {16, 1100}, "right_side"}
      ,// 1rst platform
      {{70, 150}, {150, 32}, "platform_1"},
      {{240, 280}, {140, 32}, "platform_2", b2_staticBody, Textures::SceneID::Grass, EntityType::Climbable},
      {{380, 380}, {150, 32}, "platform_3", b2_staticBody, Textures::SceneID::Grass, EntityType::Climbable},
      {{500, 500}, {320, 32}, "platform_4"},
      {{720, 420}, {120, 32}, "platform_5"},
      {{780, 300}, {120, 32}, "platform_6"},
      {{720, 600}, {120, 32}, "platform_7"},
      {{780, 700}, {120, 32}, "platform_8"},
      {{300, 100}, {50, 50}, "box_pushable_1", b2_dynamicBody, Textures::SceneID::BoxPushable}
   }},
    {ELevel::Level2,
     {{{600, 800}, {1200, 20}, "floor"},
      {{8, 580}, {16, 820}, "left_side"},
      {{1190, 580}, {16, 1100}, "right_side"}, // 1rst platform
      {{110, 150}, {220, 32}, "platform_1"},
      {{450, 150}, {250, 32}, "platform_2"},
      {{390, 100}, {32, 100}, "grate_1", b2_staticBody, Textures::SceneID::Grate},
      {{380, 370}, {200, 32}, "platform_3"},
      {{400, 364}, {60, 20}, "button_1", b2_staticBody, Textures::SceneID::Button},
      {{1100, 800}, {60, 20}, "button_2", b2_staticBody, Textures::SceneID::Button},
      {{496, 345}, {32, 85}, "platform_3_wall"},
      {{591, 435}, {32, 595}, "middle_wall"},
      {{750, 100}, {50, 50}, "box_pushable_1", b2_dynamicBody, Textures::SceneID::BoxPushable},
      {{900, 280}, {50, 50}, "box_pushable_2", b2_dynamicBody, Textures::SceneID::BoxPushable},
      {{60, 800}, {60, 20}, "teleport_1", b2_staticBody, Textures::SceneID::Teleport},
      {{870, 800}, {60, 20}, "teleport_2", b2_staticBody, Textures::SceneID::Teleport},
      {{950, 385}, {250, 32}, "platform_4", b2_staticBody, Textures::SceneID::Grass},
   }}
  };
};
