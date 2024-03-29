#pragma once
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Events/EventEnums.h>
#include <SFML/Graphics.hpp>
#include <variant>
#include <vector>

using AnimationId = std::variant<Textures::CharacterID, Textures::SceneBGAni>;

namespace RyuAnimator::AnimationSpec {
    // although the information for Frame & TaggedAnimation could be better consolidated
    // this is done this way bc the input json file-format from aseprite demands it
    // TODO: but we could use standard start values for other fields as well as we do with Event
    struct Frame 
    {
      int16_t duration;
      int16_t height;
      int16_t width;
      int16_t x; /// x-position in spritesheet
      int16_t y; /// y-position in spritesheet
      Ryu::EEvent event;
/*TODO: conflict ?
      Frame() :
        duration(0)
       ,height(0)
       ,width(0)
       ,x(0)
       ,y(0)
       ,event(Ryu::EEvent::None) {} */
    };

    struct Animation
    {

      std::string name;
      int16_t fromFrame; /// Frame Startposition in spritesheet
      int16_t toFrame; /// Frame Endposition in spritresheet
      std::string direction;
      std::vector<Frame> frames;
      std::size_t numFrames; // == TaggedAnimation::toFrame - TaggedAnimation::fromFrame
      sf::Vector2i frameSize;
      sf::Time animationDuration;
      bool repeat;
      Textures::AnimationType animationType;
      AnimationId animationId;
  
      Animation() : 
           name("name")
          ,fromFrame(0)
          ,toFrame(0)
          ,direction("forward")
          ,frames()
          ,numFrames(0)
          ,frameSize()
          ,animationDuration()
          ,repeat(false)
          ,animationType(Textures::AnimationType::Character)
          ,animationId(Textures::CharacterID::None) {}
  
    };
    struct Spec{
      std::string specName;
      std::string spritesheetName;
      std::vector<Animation> animations; 
    }; 
  
} /// namespace AnimationSpec:RyuAnimator    
