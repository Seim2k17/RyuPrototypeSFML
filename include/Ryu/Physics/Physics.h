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
        Textures::SceneID textureId, /* = Textures::SceneID::Grass,*/ EntityType entityType, int layer = 1, /*=EntityType::None);*/
        float rotation = 0.f); /* radians, ccw */

    b2Vec2 mPosition;
    b2Vec2 mSize;
    /// rotation around the body center (radians, ccw); persisted in the
    /// level JSON as "rotationDeg" and applied on (re)creation of the body
    float mRotation;
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
    /// rect of a character physics body: center in world meters + live shape
    /// extents in pixels (duck/stand shape aware), used by the level editor
    struct CharacterPhysicsRect {
        b2Vec2 centerM;
        sf::Vector2f sizePx;
    };

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
    /// rebuilds the character body with the duck/non-duck shape, keeping the
    /// FEET anchored at the old body's bottom edge (called when the duck
    /// state changes); needed since Physics owns all bodies
    void recreateCharacterPhysics(ICharacter& character, bool inDuckMode);
    /// rect of the character physics body (duck/stand shape aware) for the
    /// level editor
    CharacterPhysicsRect getCharacterRect(ECharacters character) const;
    /// teleports the character body horizontally/vertically (transform +
    /// zeroed velocity; the debug shape is kept in sync); no state change
    void setCharacterPositionPx(ECharacters character,
                                const sf::Vector2f& positionPx);
    void setDebugDrawer(b2DrawSFML dbgDrawer);
    void setDebugPhysics(bool debugPhysics);
    void update();

    void onBodyCreated(BodyCreatedCallback callback);
    void onBodyDestroyed(BodyDestroyedCallback callback);
    void onBodyTransformed(BodyTransformedCallback callback);
    void clearCharacterPhysics();

    /** \name Level editing
     *  Incremental level manipulation used by the level editor.
     *  \note   Body removal must only ever be done here: Physics owns all
     *          Box2D bodies (double-destroy hits the B2_ASSERT in Box2D).
     *  \{ */
    /** Creates the physics body of a new scene object and fires the
        ObjectCreated event; the object is appended to sceneObjects \p level. */
    SceneObjectPhysicsParameters& addSceneObject(ELevel level,
                                                 SceneObjectPhysicsParameters object);
    /** Removes a scene object (body + parameters); fires ObjectDestroyed.
        Returns false if no object with this bodyId exists. */
    bool removeSceneObject(b2BodyId bodyId);
    /** Recreates the body of an edited object (position/size/type changes);
        returns the (new) bodyId. */
    b2BodyId recreateSceneObject(b2BodyId bodyId);
    /** Returns the parameters of a scene object, nullptr if unknown. */
    SceneObjectPhysicsParameters* findSceneObject(b2BodyId bodyId);
    /** Destroys and recreates all scene-object bodies of \p level (e.g. after
        editing or loading level data). The character bodies are untouched. */
    void reloadSceneObjects(ELevel level);
    /** Moves a scene object live during a drag (no body recreation);
        returns false if unknown. */
    bool moveSceneObject(b2BodyId bodyId, const b2Vec2& position);
    /** Rotates a scene object live around the world-space pivot \p pivotM
        (no body recreation): the body center travels on an arc around the
        pivot so the object turns around it. Returns false if unknown. */
    bool rotateSceneObject(b2BodyId bodyId, float angleRadians,
                           const b2Vec2& pivotM);
    /** Current rotation (radians) of a scene-object body; 0 if unknown. */
    float getSceneObjectRotation(b2BodyId bodyId) const;
    /** The level whose scene-object bodies are currently created. */
    ELevel getActiveLevel() const { return mActiveLevel; }
    /** \} */

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
    ELevel mActiveLevel;
    float mPhTimeStep;
    bool mDebugPhysicsActive;

    std::vector<BodyCreatedCallback> mBodyCreatedCallbacks;
    std::vector<BodyDestroyedCallback> mBodyDestroyedCallbacks;
    std::vector<BodyTransformedCallback> mBodyTransformedCallbacks;

   public: // TODO: tmp storage !!!! -> make static again outside of class, e.g. mnovce to renderer-class
    std::map<ELevel, std::vector<SceneObjectPhysicsParameters>> sceneObjects;
};
