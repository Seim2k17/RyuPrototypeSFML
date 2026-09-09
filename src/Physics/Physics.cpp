#include "Ryu/Core/AssetIdentifiers.h"
#include "Ryu/Debug/b2DrawSFML.hpp"
#include "Ryu/Events/EventEnums.h"
#include "Ryu/Scene/EntityStatic.h"
#include "Ryu/Scene/SceneEnums.h"
#include <Ryu/Physics/Physics.h>
#include <Ryu/Events/PhysicsEvents.h>
#include <Ryu/Events/EventBus.h>

#include <Ryu/Core/Utilities.h>

#include <Ryu/Character/ICharacter.h>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Shape.hpp>
#include <SFML/System/Vector2.hpp>
#include <box2d/box2d.h>
#include <box2d/types.h>
#include <exception>
#include <fmt/core.h>
#include <memory>


const b2Vec2 GRAVITY(0,9.81f);
constexpr float PHYSICS_TIME_STEP = 1.f / 60.f;
constexpr int SUBSTEP_COUNT = 1;
constexpr int32_t VELOCITY_ITERATIONS = 8;
constexpr int32_t POSITION_ITERATIONS = 3;

CharacterPhysicsParameters::CharacterPhysicsParameters() :
    mBody(nullptr),
    mFixture(nullptr),
    mGravityScale(4.8f),  /// for dynamic objects density needs to be > 0
    mDensity(5.f), /// for dynamic objects density needs to be > 0
    mFriction(0.1f), /// recommended by  b2d docu
    mFixtureRestitution(0.1f),
    mRaycastLength(40.0f),
    mMoveMultiplier({1.05f, 1.47f}),
    mJumpForwardImpulse({150, -250}),
    mJumpUpImpulse({0, -200}),
    mMassCenter({0, 0}),
    mBodyMass(25)
{}

CharacterPhysicsParameters::~CharacterPhysicsParameters()
{
    b2DestroyBody(mBodyId);
    mBodyId = b2_nullBodyId;
}

SceneObjectPhysicsParameters::SceneObjectPhysicsParameters() :
    mPosition({}),
    mSize({}),
    mName(""),
    mType(b2BodyType::b2_staticBody),
    mTextureId(Textures::SceneID::Unknown),
    mEntityType(EntityType::None),
    mPhysicsBodyId(b2_nullBodyId) {}
    // polygonShape.SetAsBox(0.5,0.9);


SceneObjectPhysicsParameters::SceneObjectPhysicsParameters(
        b2Vec2 positionInPixel, b2Vec2 sizeInPixel,
        std::string name, b2BodyType type,
        Textures::SceneID textureId, EntityType entityType) :
    mPosition(Converter::pixelsToMeters(positionInPixel.x), Converter::pixelsToMeters(positionInPixel.y)),
    mSize(Converter::pixelsToMeters(sizeInPixel.x), Converter::pixelsToMeters(sizeInPixel.y)),
    mName(name),
    mType(type),
    mTextureId(textureId),
    mEntityType(entityType),
    mPhysicsBodyId(b2_nullBodyId)
{}

// TODO: set contactlistener (see characterbase)
Physics::Physics() :
    mCharacterPhysics({}),
    mPhysicsWorldId(),
    mStaticEntities(),
    mPhTimeStep(PHYSICS_TIME_STEP),
    mDebugPhysicsActive(false)
{
    b2WorldDef worldDef = b2DefaultWorldDef();
    worldDef.gravity = GRAVITY;
    mPhysicsWorldId = b2CreateWorld(&worldDef);
    
}

Physics::~Physics()
{
   for(auto& level : sceneObjects)
   {
       for(auto& sceneObj : level.second)
       {
           // TODO: save every bodyId in a map
           // destzroy every stored physical sceneobject
           // b2DestroyBody(sceneObj.mBodyId);
           // mPhysicsWorld->DestroyBody(sceneObj.mPhysicsBody);
       }
   }

    for (auto& [name, charPhysics] : mCharacterPhysics) {
        PhysicsObjectDestroyedEvent event;
        event.bodyId = charPhysics.mBodyId;
        event.name = name;
        EventBus::emit(Ryu::EPhysicsEvent::CharacterDestroyed, event);
    }

    b2DestroyWorld(mPhysicsWorldId);
    mPhysicsWorldId = b2_nullWorldId;
}

void
Physics::update()
{
    b2World_Step(mPhysicsWorldId, PHYSICS_TIME_STEP, SUBSTEP_COUNT); //VELOCITY_ITERATIONS, POSITION_ITERATIONS);

    // Emit update events for dynamic bodies
    for (auto& [name, charPhysics] : mCharacterPhysics) {
        if (!b2Body_IsValid(charPhysics.mBodyId)) continue;

        // Skip if not dynamic or sleeping
        if (b2Body_GetType(charPhysics.mBodyId) != b2_dynamicBody) continue;
        if (!b2Body_IsAwake(charPhysics.mBodyId)) continue;
            auto position = b2Body_GetPosition(charPhysics.mBodyId);
            auto rotation = b2Rot_GetAngle(b2Body_GetRotation(charPhysics.mBodyId));

            PhysicsObjectUpdatedEvent event;
            event.bodyId = charPhysics.mBodyId;
            event.position = position;
            event.rotation = rotation;
            EventBus::emit(Ryu::EPhysicsEvent::CharacterUpdated, event);
    }

    // --- Emit updates for dynamic SCENE OBJECTS when they're transformed (e.g., falling boxes) ---
    // TODO: here logic is ill:
    // we differ btw. sceneObjects and physicalObjects
    // but here its not given
    // we should check and also set the layer (== SpriteNode and which layer): seee mSceneLayers in World.cpp
    // 1. objects which are cosmetic or levelassets without collision & static
    // 2. objects which are levelassets with collision & static
    // 3. objects which are cosmetic or levelassets without collision & "dynamic" (moving sprites, animations)
    // 4. objects which are levelassets with collision and moving (platforms)
    // 5. characterassets with collision (mainly static)
    for (auto& [level, objects] : sceneObjects) {
        for (auto& obj : objects) {
            if (!b2Body_IsValid(obj.mPhysicsBodyId)) continue;

            // Skip if not dynamic or sleeping
            if (obj.mType != b2_dynamicBody) continue;
            if (!b2Body_IsAwake(obj.mPhysicsBodyId)) continue;

            PhysicsObjectUpdatedEvent event;
            event.bodyId = obj.mPhysicsBodyId;
            event.name = obj.mName;
            event.position = b2Body_GetPosition(obj.mPhysicsBodyId);
            event.rotation = b2Rot_GetAngle(b2Body_GetRotation(obj.mPhysicsBodyId));
            EventBus::emit(Ryu::EPhysicsEvent::ObjectUpdated, event);
        }
    }
    
}

// TODO: check if these are needable when using the EventBus-approach 
void
Physics::onBodyCreated(BodyCreatedCallback callback)
{
    mBodyCreatedCallbacks.push_back(callback);
}

void
Physics::onBodyDestroyed(BodyDestroyedCallback callback)
{
    mBodyDestroyedCallbacks.push_back(callback);
}

void
Physics::onBodyTransformed(BodyTransformedCallback callback)
{
    mBodyTransformedCallbacks.push_back(callback);
}


void
Physics::setDebugPhysics(bool debugPhysics)
{
    mDebugPhysicsActive = debugPhysics;
}

void
Physics::debugDrawSegment(b2Vec2 const &p1, b2Vec2 const &p2,
                          b2HexColor const &color) const
{
    if (mDebugPhysicsActive)
    {
        debugDrawer.DrawSegment(p1, p2, color);
    }
}

void
Physics::debugDraw() const
{
    if(mDebugPhysicsActive)
    {
        // TODO: how to draw physics ?
        // mPhysicsWorld->DebugDraw();
    }
}

void
Physics::draw(sf::RenderWindow& window)
{
/*
    // TODO: mPhysicsBody is nullptr but is set in CreatePhysicsBody!
    // thats why thy bodies cant be displayed
    // so probably not set correctly or destroyed when going out of scope
    // before the refactoring the physicsbodies were emplaced in an physicsbodyarray
    // and created there directly
    // TODO: next step ask with help on ChatGTP what could be wrong in this situation
    
    if (sceneObjects.size() > 0) {
        for (auto& obj : sceneObjects.at(ELevel::Level2)) {
            fmt::print("try to draw {}, with PhysicsBodyId: {} , size {}/{} , position {}/{} \n"
                       , obj.mName
                       , B2_IS_NULL(obj.mPhysicsBodyId) ? "not there" : std::to_string(obj.mPhysicsBodyId.index1)
                       , obj.mSize.x
                       , obj.mSize.y
                       , obj.mPosition.x
                       , obj.mPosition.y
            );
            auto shape = getShapeFromPhysicsBody(obj.mPhysicsBodyId);
            
            if (shape == nullptr) {
                fmt::print("shape ptr seems to be null\n");
                fmt::print("damn\n");
                //return;
            }
            else
            {
                window.draw(*(shape));
            }
        }
        fmt::print("#### next frame ####\n");
        fmt::print("\n");
        fmt::print("\n");
    }
  */  
}

sf::Shape*
Physics::getShapeFromPhysicsBody(b2BodyId physicsBodyId) {
    if (not b2Body_IsValid(physicsBodyId) || B2_IS_NULL(physicsBodyId))
    {
        fmt::print("Body is null or invalid! \n");
        return nullptr;
    }

    void* userData = b2Body_GetUserData(physicsBodyId);

    if (not userData)
    {
        fmt::print("Body UserData for id: {} is null! \n", physicsBodyId.index1);
        return nullptr;
    }
    
    //auto entity = reinterpret_cast<EntityStatic*>(user_data);
    /*
    auto body = reinterpret_cast<uintptr_t>(physicsBody);
    sf::Shape* shape =
    reinterpret_cast<sf::RectangleShape*>(mStaticEntities.at(body)->getShape());
    */

    sf::Shape* shape =
        static_cast<sf::RectangleShape*>(userData);
        //reinterpret_cast<sf::RectangleShape*>(entity->getShape());

    if (shape) {

        try {
            auto bodyPos = b2Body_GetPosition(physicsBodyId);
            auto bodyRot = b2Body_GetRotation(physicsBodyId);
            float angleInRadians = b2Rot_GetAngle(bodyRot);
            shape->setPosition({
                bodyPos.x,
                bodyPos.y});
            shape->setRotation(sf::radians(angleInRadians));
        } catch (std::exception) {
            fmt::print("No shape.\n");
        }
    } else {
        fmt::print("shape null.\n");
        return nullptr;
    }
    return shape;
}

void
Physics::initCharacterPhysics(ICharacter& character, bool inDuckMode)
 {
// TODO: CAUTION:: THERE is an thinking error and a cyclic dependency inside, TOO LATE !!!
   // TODO:
   // - initialize needed values in iniatlizer list of ctor
   // - get implementation from CharacterBase::initPhysics(...)
   // - call accordingly
   //-    // TODO: make it adjustable ? or remove and add new ? -> e.g. duck state ->
    // halfPhysics box
    // init physics after the charactersprite was created !
    // Create the body of the falling Crate
    auto position = character.getPosition();

    const auto shapeSize = inDuckMode ? DUCK_FRAME_SIZE : INIT_FRAME_SIZE;
    int size_x
        = shapeSize
              .first; // mCharacterAnimation.getSprite().getTextureRect().width;
    int size_y = shapeSize.second;

    CharacterPhysicsParameters& charPhysics = mCharacterPhysics.at(character.getCharacterName());

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.type = b2_dynamicBody; /// TODO: or even kinematic body ?
    bodyDef.position.x = Converter::pixelsToMeters<double>(position.x);
    bodyDef.position.y = Converter::pixelsToMeters<double>(
                             inDuckMode
                                 ? position.y + (DUCK_FRAME_SIZE.second / 2)
                                 : position.y);
    bodyDef.fixedRotation = true;
    bodyDef.gravityScale = charPhysics.mGravityScale;

    charPhysics.mBodyId = b2CreateBody(mPhysicsWorldId, &bodyDef);
    
    // Create a the shape
    b2Polygon polygonBoxShape = b2MakeBox(Converter::pixelsToMeters<double>(size_x * 0.5f), Converter::pixelsToMeters<double>(size_y * 0.5f));
    b2ShapeDef shapeDef = b2DefaultShapeDef(); // initialize shapedefinition
    shapeDef.density = charPhysics.mDensity;
    //shapeDef.friction = charPhysics.mFriction; deprecated since Box2D 3.1, different concept


    //shapeDef.userData = (uintptr_t)shape;

    charPhysics.mShapeDef = shapeDef;
    charPhysics.mShapeId = b2CreatePolygonShape(charPhysics.mBodyId, &shapeDef, &polygonBoxShape);
    
    // SFML shape 
    // TODO: with SFML 3 there are probably smartPointer more elegant ? otherwise do RAII !
    sf::Shape *shape = new sf::RectangleShape(sf::Vector2f(size_x, size_y));
    // or can we delete this/free this at the end of the function ? bc its casted to uintptr_t
    // TODO: test it
    // std::unique_ptr<sf::Shape> shape =
    // std::make_unique<sf::RectangleShape>(sf::Vector2f(size_x, size_y));
    shape->setOrigin({size_x / 2.0f, size_y / 2.0f});
    shape->setPosition(sf::Vector2f(position.x, position.y));
    // TODO check how we set a texture
    //shape->setTexture(
    //    &baseTextureManager.getResource(Textures::PhysicAssetsID::Empty));

    b2Body_SetUserData(charPhysics.mBodyId, &shape);
    //b2ShapeId shapeId = b2Body_AddShape(charPhysics.mBodyId, )
    
    fmt::print("Init character at position {},{}\n",
               Converter::metersToPixels(bodyDef.position.x),
               Converter::metersToPixels(bodyDef.position.y));
    // mBody->SetLinearVelocity(b2Vec2(0.0f, -50.0f));

    // insert physics according to character
    mCharacterPhysics.insert(
        std::make_pair(character.getCharacterName(), charPhysics)
    );

    // Emit event for new physics object
    PhysicsObjectCreatedEvent event;
    event.bodyId = charPhysics.mBodyId;
    event.shapeId = charPhysics.mShapeId;
    event.name = character.getCharacterName();
    event.size = {size_x, size_y};
    event.isDynamic = true;
    event.textureId = Textures::SpritesheetID::Ichi80x96;  // StartSpritesheet is the normal one
    
    EventBus::emit(Ryu::EPhysicsEvent::CharacterCreated, event);

    // EventBus::emit(Ryu::EPhysicsEvent::ObjectCreated, event);
}

void
Physics::createPhysicsSceneObjects(ELevel level)
{
    int i = 0;
    for(auto& obj : sceneObjects.at(level))
    {
        fmt::print("create: {} \n", obj.mName);
        obj.mPhysicsBodyId = createPhysicsBody(obj, i);
        fmt::print("After set: Physicsbody for {} set ,{} with ID: {}. \n",obj.mName
                   , B2_IS_NULL(obj.mPhysicsBodyId) ? "null_BodyId" : "physBody exists"
                   , obj.mPhysicsBodyId.index1);
        // TODO: still relevant ?
        // phGroundBodies.emplace_back(PhysicsObject("", createPhysicalBox(obj)));
    }
}

// TODO: make method acc Box2D v3.1 !
// and also clean / make it easier !, see InitCharacterPhysics, check what can be combined
// lambda or helpermethods
b2BodyId
Physics::createPhysicsBody(SceneObjectPhysicsParameters& sceneObject, int& i)
//Physics::createPhysicsBody(SceneObjectPhysicsParameters sceneObject)
{
//    b2Vec2 objPosition = b2Vec2(Converter::pixelsToMeters(sceneObject.mPosition.x)
//                               ,Converter::pixelsToMeters(sceneObject.mPosition.y));

    b2Vec2 objPosition = sceneObject.mPosition;

    b2Vec2 objSize = sceneObject.mSize;

    b2BodyDef bodyDef = b2DefaultBodyDef();
    bodyDef.position = (b2Vec2){Converter::metersToPixels(objPosition.x), Converter::metersToPixels(objPosition.y)};
    bodyDef.type = sceneObject.mType;
    
    if(B2_IS_NULL(mPhysicsWorldId)){
        fmt::print("no physics world created\n");
        return b2_nullBodyId;
    }

    b2BodyId bodyId = b2CreateBody(mPhysicsWorldId, &bodyDef);

    b2ShapeDef shapeDef = b2DefaultShapeDef(); // initialize shapedefinition
    shapeDef.density = 2.0; // TODO make it configurable ?
    // shapeDef.friction = 0.98; // deprecated since Box2D 3.1
    
    b2Filter filter = b2DefaultFilter();
    filter.categoryBits = CollisionCategories::STATIC;  // This is a static object
    filter.maskBits = CollisionCategories::DYNAMIC | CollisionCategories::PLAYER;  // Collide with dynamic and player
    filter.groupIndex = 0;
    
    shapeDef.filter = filter;

    b2SurfaceMaterial material = b2DefaultSurfaceMaterial();
    material.restitution = 0.2f;  // Bounciness (0 = no bounce, 1 = perfect bounce)
    material.friction = 0.3f;     // Friction coefficient

    b2Polygon box = b2MakeBox((objSize.x / 2.0), (objSize.y / 2.0));

    b2ShapeId shapeId = b2CreatePolygonShape(bodyId, &shapeDef, &box);
    b2Shape_SetSurfaceMaterial(shapeId, material);

    // std::unique_ptr<sf::RectangleShape> shape =
    // std::make_unique<sf::RectangleShape>(sf::Vector2f(size_x,size_y));
    // sf::RectangleShape shape{sf::Vector2f(size_x,size_y)};
    // TODO remove sfml dependency
    // use smartpointer ?
    sf::Vector2f sfml_size{Converter::metersToPixels(objSize.x), Converter::metersToPixels(objSize.y)};
    sf::Shape *shape = new sf::RectangleShape(sfml_size);
    //std::unique_ptr<sf::Shape> shape =
    //    std::make_unique<sf::RectangleShape>(sf::Vector2f(objSize));

    shape->setOrigin({(float)(Converter::metersToPixels(objSize.x) / 2.0), (float)(Converter::metersToPixels(objSize.y) / 2.0)});
    shape->setPosition(sf::Vector2f(Converter::metersToPixels(objPosition.x), Converter::metersToPixels(objPosition.y)));

    if (sceneObject.mTextureId != Textures::SceneID::Unknown) {
         //shape->setFillColor(sf::Color::Red);
         shape->setOutlineColor(sf::Color::Red);
         shape->setOutlineThickness(2.0f);
        // TODO: how to set texture without coupling to texturestuff
        // this can be probably be part of the Levelmanager and to LM we need a link
        // from World and from Physics ... ? MAYBE
        //shape->setTexture(&mSceneTextures.getResource(texture));
    }
    else {
        shape->setFillColor(sf::Color::Green);
    }

    
    // TODO: check what is staticEntity for
    auto staticEntity = std::make_unique<EntityStatic>(sceneObject.mEntityType);
    // std::shared_ptr<EntityStatic> staticEntity =
    // std::make_shared<EntityStatic>());
    staticEntity->setName(sceneObject.mName);

    auto shapePosition = shape->getGlobalBounds().position;
    auto shapeSize = shape->getGlobalBounds().size;
    std::vector<sf::Vector2f> cornerPoints{
        {shapePosition.x, shapePosition.y},
        {shapePosition.x + shapeSize.x, shapePosition.y},
        {shapePosition.x, shapePosition.y + shapeSize.y},
        {shapePosition.x + shapeSize.x,
         shapePosition.y + shapeSize.y}};

    staticEntity->setCornerPoints(cornerPoints);
    // TODO: is this up to date and needable ?
    //staticEntity->setShape(std::unique_ptr<sf::Shape>(shape));
    
    // res->GetUserData().pointer = reinterpret_cast<uintptr_t>(staticEntity.get());
    //mStaticEntities[reinterpret_cast<uintptr_t>(res)] = staticEntity;

    //sf::Shape* rawShape = shape.release();
    
    //auto staticShape = mStaticEntities[bodyId]->getShape();
    b2Body_SetUserData(bodyId, shape);

    auto userData = b2Body_GetUserData(bodyId);

    if(userData)
    {
       fmt::print("Getting userData directly is null.\n"); 
    }
    else
    {
       fmt::print("UserData is there.\n");
        
    }
    i++;
    // TODO: keep this not in the physics-class !!! -> renderer ? 
    //mStaticEntities[bodyId]->setShape(std::unique_ptr<sf::Shape>(shape));

    // Emit event for new physics object
    PhysicsObjectCreatedEvent event;
    event.bodyId = bodyId;
    event.shapeId = shapeId;
    event.name = sceneObject.mName;
    //TODO: convert correctly
    event.size = sceneObject.mSize;
    event.isDynamic = (sceneObject.mType == b2_dynamicBody);
    event.textureId = sceneObject.mTextureId;  // Pass texture ID for rendering
    
    EventBus::emit(Ryu::EPhysicsEvent::ObjectCreated, event);
    return bodyId;
}

void
Physics::setDebugDrawer(b2DrawSFML dbgDrawer)
{
    /* TODO: How to set the Tags in Box2d v3.1 ?
    debugDrawer.SetTarget(dbgDrawer.GetRenderTarget());
    debugDrawer.SetScale(dbgDrawer.GetScale());
    debugDrawer.SetFlags(dbgDrawer.GetFlags());
    mPhysicsWorld->SetDebugDraw(&debugDrawer);// was located as static in World
    */
}
