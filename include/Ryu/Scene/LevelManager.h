#pragma once

#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Scene/EntityStatic.h>
//#include <Ryu/Scene/LevelObject.h>
//#include <Ryu/Physics/PhysicsObject.h>

#include <box2d/b2_body.h>
#include <map>
#include <vector>
#include <string>


struct LevelObject
{
    LevelObject(int x, int y, int size_x, int size_y, std::string name,
                b2BodyType type
                , Textures::SceneID texture = Textures::SceneID::Unknown
                , EntityType entityType = EntityType::None)
    :  posX(x)
        , posY(y)
        , sizeX(size_x)
        , sizeY(size_y)
        , name(name)
        , type(type)
        , texture(texture)
        , entityType(entityType)
    {}

    int posX;
    int posY;
    int sizeX;
    int sizeY;
    std::string name;
    b2BodyType type;
    Textures::SceneID texture;
    EntityType entityType;
};

static std::map<std::string, std::vector<LevelObject>> physicsObjects = {
//TODO: get physicsType with Name of the Object from Physics-Class (mScenePhysics)
{"Level1",
     {{600, 780, 1200, 20, "floor", b2_staticBody},
      {8, 580, 16, 800, "left_side", b2_staticBody},
      {1190, 580, 16, 1100, "right_side", b2_staticBody}
      ,// 1rst platform
      {70, 150, 150, 32, "platform_1", b2_staticBody, Textures::SceneID::Grass},
      {240, 280, 140, 32, "platform_2", b2_staticBody, Textures::SceneID::Grass,
       EntityType::Climbable},
      {380, 380, 150, 32, "platform_3", b2_staticBody, Textures::SceneID::Grass,
       EntityType::Climbable},
      {500, 500, 320, 32, "platform_4", b2_staticBody,
       Textures::SceneID::Grass},
      {720, 420, 120, 32, "platform_5", b2_staticBody,
       Textures::SceneID::Grass},
      {780, 300, 120, 32, "platform_6", b2_staticBody,
       Textures::SceneID::Grass},
      {720, 600, 120, 32, "platform_7", b2_staticBody,
       Textures::SceneID::Grass},
      {780, 700, 120, 32, "platform_8", b2_staticBody,
       Textures::SceneID::Grass},
      {300, 100, 50, 50, "box_pushable_1", b2_dynamicBody,
       Textures::SceneID::BoxPushable}}},
    {"Level2",
     {{600, 800, 1200, 20, "floor", b2_staticBody}
      ,{8, 580, 16, 820, "left_side", b2_staticBody}
      ,{1190, 580, 16, 1100, "right_side", b2_staticBody} // 1rst platform
      ,{110, 150, 220, 32, "platform_1", b2_staticBody, Textures::SceneID::Grass}
      ,{450, 150, 250, 32, "platform_2", b2_staticBody, Textures::SceneID::Grass}
      ,{390, 100, 32, 100, "grate_1", b2_staticBody, Textures::SceneID::Grate}
      ,{380, 370, 200, 32, "platform_3", b2_staticBody, Textures::SceneID::Grass}
      ,{400, 364, 60, 20, "button_1", b2_staticBody, Textures::SceneID::Button}
      ,{1100, 800, 60, 20, "button_2", b2_staticBody, Textures::SceneID::Button}
      ,{496, 345, 32, 85, "platform_3_wall", b2_staticBody}
      ,{591, 435, 32, 595, "middle_wall", b2_staticBody}
      ,{750, 100, 50, 50, "box_pushable_1", b2_dynamicBody, Textures::SceneID::BoxPushable}
      ,{900, 280, 50, 50, "box_pushable_2", b2_dynamicBody, Textures::SceneID::BoxPushable}
      ,{60, 800, 60, 20, "teleport_1", b2_staticBody, Textures::SceneID::Teleport}
      ,{870, 800, 60, 20, "teleport_2", b2_staticBody, Textures::SceneID::Teleport}
      ,{950, 385, 250, 32, "platform_4", b2_staticBody, Textures::SceneID::Grass}
     }
   }
};



class LevelManager
{
public:
    LevelManager(){}
//private:
//    std::map<std::string, std::vector<PhysicsObject>> physicsObjects;

};
