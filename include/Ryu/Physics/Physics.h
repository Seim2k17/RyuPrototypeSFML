/**
 * This class is responsible for all physicsrelated stuff for Characters and for SceneObjects
 * @author: Simon Nguyen
 * @date: 02/02/2025
 *
 * */

#pragma once

#include "Ryu/Events/Subject.h"
#include "Ryu/Scene/SceneEnums.h"
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Control/CharacterEnums.h>
#include <box2d/b2_world.h>
#include <box2d/b2_body.h>
#include <memory>
#include <map>

class CharacterBase;

struct CharacterPhysicsParameters{
    b2Body* mBody;
    b2Fixture* mFixture;
    float mGravityScale;
    float mFixtureDensity;
    float mFixtureFriction;
    float mFixtureRestitution;
    float mRaycastLength;
    CharacterBase& mCharacter;
};

struct SceneObjectPhysicsParameters
{
    b2BodyType type;
};

class Physics : public Subject {
   public:
    void addSceneObjectToPhysics(ELevel, std::string name);
    void initCharacterPhysics(ECharacters character);

   private:
    std::shared_ptr<b2World> mPhysicsWorld;
    std::map<ECharacters, CharacterPhysicsParameters> mCharacterPhysics;
    std::map<std::string, SceneObjectPhysicsParameters> mScenePhysics; // see also physicsObject @LevelManager.h
                                                                       // key is a combination of LevelName (ELevel.string) and ObjectName

};
