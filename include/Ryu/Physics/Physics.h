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
#include <box2d/box2d.h>
#include <memory>
//#include <map>
#include <unordered_map>

class CharacterBase;
class ICharacter;
class RenderWindow;
class Shape;

extern b2DrawSFML debugDrawer;

struct CharacterPhysicsParameters{
    CharacterPhysicsParameters();
    ~CharacterPhysicsParameters();

    b2Body* mBody;
    b2BodyId mBodyId; // maybe only save BodyId ?
    [[deprecated]] b2Fixture* mFixture; // since Box2d v3.1 the fixture-concept is gone
    b2ShapeId mShapeId;
    b2ShapeDef mShapeDef;
    // these are stored inside mShapeDef as well ! (since Box2D v3.1)
    float mGravityScale;
    float mDensity;
    float mFriction;
    [[deprecated]] float mFixtureRestitution;

    float mRaycastLength;
    // due increase of gravityscale (falling is then more gamey) the physicbody
    // needs some adustments for movement so its not behind the movement of
    // the characteranimation
    b2Vec2 mMoveMultiplier;

    b2Vec2 mJumpForwardImpulse;
    b2Vec2 mJumpUpImpulse;
    b2Vec2 mMassCenter;
    float mBodyMass;
};

struct SceneObjectPhysicsParameters
{
    SceneObjectPhysicsParameters();
    SceneObjectPhysicsParameters(
        b2Vec2 position, b2Vec2 size,
        std::string name, b2BodyType type, /* = b2_staticBody,*/
        Textures::SceneID textureId, /* = Textures::SceneID::Grass,*/ EntityType entityType, int layer = 1); /*=EntityType::None);*/

    b2Vec2 mPosition;
    b2Vec2 mSize;
    std::string mName;
    b2BodyType mType;
    Textures::SceneID mTextureId;
    EntityType mEntityType;
    b2BodyId mPhysicsBodyId;
    // TODO: use existing enum for more clarity
    int mLayer; // Layer for scenegraph rendering (0=Background, 1=Ground1, 2=Foreground)
};

// Custom hash function for b2BodyId
struct b2BodyIdHash {
    std::size_t operator()(const b2BodyId& id) const {
        // Assuming b2BodyId has 'index' and 'generation' fields
        // If not, replace with the actual fields in b2BodyId
        return std::hash<uint32_t>()(id.index1) ^ std::hash<uint32_t>()(id.generation);
    }
};

// Custom equality function for b2BodyId
struct b2BodyIdEqual {
    bool operator()(const b2BodyId& lhs, const b2BodyId& rhs) const {
        // Assuming b2BodyId has 'index' and 'generation' fields
        return lhs.index1 == rhs.index1 && lhs.generation == rhs.generation;
    }
};

class Physics : public Subject {

    // use callbacks (for Observer-pattern) to notify other systems, like Renderer when physics changed
    using BodyCreatedCallback = std::function<void(b2BodyId, const SceneObjectPhysicsParameters&)>;
    using BodyDestroyedCallback = std::function<void(b2BodyId)>;
    using BodyTransformedCallback = std::function<void(b2BodyId, b2Vec2, b2Rot)>;
        
   public:
    Physics();
    ~Physics();

    void createPhysicsSceneObjects(ELevel level);
    void debugDrawSegment(const b2Vec2& p1, const b2Vec2& p2,
                          const  b2HexColor& color) const;
    void debugDraw() const;
    void draw(sf::RenderWindow& window);
    sf::Shape* getShapeFromPhysicsBody(b2BodyId physicsBodyId);
    b2WorldId& getPhysicsWorldId(){return mPhysicsWorldId;}
    void initCharacterPhysics(ICharacter& character, bool inDuckMode);
    void setDebugDrawer(b2DrawSFML dbgDrawer);
    void setDebugPhysics(bool debugPhysics);
    void update();

    void onBodyCreated(BodyCreatedCallback callback);
    void onBodyDestroyed(BodyDestroyedCallback callback);
    void onBodyTransformed(BodyTransformedCallback callback);
    void clearCharacterPhysics();

   private:
    //void createPhysicsBody(SceneObjectPhysicsParameters sceneObject);
    b2BodyId createPhysicsBody(SceneObjectPhysicsParameters& sceneObject, int& i);
    // box2d physics
    std::map<ECharacters, CharacterPhysicsParameters> mCharacterPhysics;
    b2WorldId mPhysicsWorldId;
    //b2World* mPhysicsWorld;
    // std::map<ELevel, std::vector<SceneObjectPhysicsParameters> > mSceneObjects; // see sceneObjects

    // TODO: check if we keep it here or not ? Do we even need this at all ?                                                                   // key is a combination of LevelName (ELevel.string) and ObjectName ? WHY ?
    // it was: why?
    std::unordered_map<const b2BodyId, std::unique_ptr<EntityStatic>, b2BodyIdHash, b2BodyIdEqual> mStaticEntities;
    //std::map<const uintptr_t, std::shared_ptr<EntityStatic> > mStaticEntities;
    // std::map<uintptr_t, EntityStatic > mStaticEntities;
    float mPhTimeStep;
    bool mDebugPhysicsActive;

    std::vector<BodyCreatedCallback> mBodyCreatedCallbacks;
    std::vector<BodyDestroyedCallback> mBodyDestroyedCallbacks;
    std::vector<BodyTransformedCallback> mBodyTransformedCallbacks;

   public: // TODO: tmp storage !!!! -> make static again outside of class, e.g. mnovce to renderer-class
    std::map<ELevel , std::vector<SceneObjectPhysicsParameters>> sceneObjects = {
    {ELevel::Level1,
     {{{320, 351}, {640, 9}, "floor", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{4, 261}, {9, 360}, "left_side", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{635, 261}, {9, 495}, "right_side", b2_staticBody, Textures::SceneID::Grass, EntityType::None }
      ,// 1rst platform
      {{37, 68}, {80, 14}, "platform_1", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{128, 126}, {75, 14}, "platform_2", b2_staticBody, Textures::SceneID::Grass, EntityType::Climbable },
      {{203, 171}, {80, 14}, "platform_3", b2_staticBody, Textures::SceneID::Grass, EntityType::Climbable },
      {{267, 225}, {171, 14}, "platform_4", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{384, 189}, {64, 14}, "platform_5", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{416, 135}, {64, 14}, "platform_6", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{384, 270}, {64, 14}, "platform_7", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{416, 315}, {64, 14}, "platform_8", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{160, 45}, {27, 23}, "box_pushable_1", b2_dynamicBody, Textures::SceneID::BoxPushable, EntityType::None }
   }},
    {ELevel::Level2,
     {{{320, 360}, {640, 9}, "floor", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{4, 261}, {9, 369}, "left_side", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{635, 261}, {9, 495}, "right_side", b2_staticBody, Textures::SceneID::Grass, EntityType::None }, // 1rst platform
      {{59, 68}, {117, 14}, "platform_1", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{240, 68}, {133, 14}, "platform_2", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{205, 45}, {17, 45}, "grate_1", b2_staticBody, Textures::SceneID::Grate, EntityType::None },
      {{203, 167}, {103, 14}, "platform_3", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{213, 164}, {32, 9}, "button_1", b2_staticBody, Textures::SceneID::Button, EntityType::None },
      {{576, 360}, {32, 9}, "button_2", b2_staticBody, Textures::SceneID::Button, EntityType::None },
      {{265, 155}, {17, 38}, "platform_3_wall", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{315, 196}, {17, 268}, "middle_wall", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
      {{400, 45}, {27, 23}, "box_pushable_1", b2_dynamicBody, Textures::SceneID::BoxPushable, EntityType::None },
      {{480, 126}, {27, 23}, "box_pushable_2", b2_dynamicBody, Textures::SceneID::BoxPushable, EntityType::None },
      {{32, 360}, {32, 9}, "teleport_1", b2_staticBody, Textures::SceneID::Teleport, EntityType::None },
      {{468, 360}, {32, 9}, "teleport_2", b2_staticBody, Textures::SceneID::Teleport, EntityType::None },
      {{507, 173}, {133, 14}, "platform_4", b2_staticBody, Textures::SceneID::Grass, EntityType::None },
   }}
  };
};
