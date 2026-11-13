// blub
#pragma once

#include <Ryu/Core/AssetManager.h> 
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Animation/SpritesheetAnimation.h>
#include <Ryu/Animation/EditorEnums.h>
#include <Ryu/Animation/JsonParser.h>

#include <imgui.h>
#include <Thirdparty/imgui-filebrowser/imfilebrowser.h>

#include <bits/stdint-intn.h>
#include <cstdint>
#include <map>
#include <vector>
#include <string>

#include <SFML/Graphics.hpp>

class SpritesheetAnimation;

using GuiCharTextureManager = AssetManager<sf::Texture, Textures::LevelID>;
using GuiTextureManager = AssetManager<sf::Texture, Textures::GuiID>;

namespace RyuAnimator
{

// fields important for every Animation 
// TODO (if Editor in use: delete struct in CharacterBase.h)
// it'll also included in AnimationSpec::Animation
struct AnimationConfig
{
    sf::Vector2i frameSize;
    sf::Vector2i startFrame;
    std::size_t numFrames;
    sf::Time duration;
    bool repeat;
    Textures::CharacterID animationId;
};

enum class EFileBrowserState
{
  None,
  SpriteSheetJson,
  AnimationConfigJson
};

enum class ImportFormat {
  None,
  Aseprite,
  Texturepacker
};

using TaggedSheetAnimation = std::pair<const std::string, std::vector<RyuParser::AnimationEditor>>  ;

  class Editor
  {
    public:
      Editor();
      ~Editor();

    //private:

      void update(sf::Time dt);

      void initData();
  
      void calculateAnimationDuration(RyuParser::AnimationEditor& ani);
  
      void createEditorWidgets(bool* p_open);

      void parseJsonData(std::string path);
      void parseJsonFile();
      bool showAnimationEditor;
  
      void exportAnimationDetailsToFile(char* JsonFilename);
      
      void setFrameDetails(int selectedAni, TaggedSheetAnimation& sheet, int frameNumber, RyuParser::AnimationEditor& ani);
      
      void setAnimationPreferences(std::string sheetName);

      void createAnimationDetails(int selectedAni, TaggedSheetAnimation& sheet );

      // add a new animation built from the frames of the currently parsed
      // spritesheet (frame indices from..to, aseprite frame numbering)
      void addAnimation(const std::string& name, int fromFrame, int toFrame);
      void deleteAnimation(int index);
      void setSpritesheetAnimationDetails(const AnimationConfig& config, sf::Time aniDuration, std::vector<RyuParser::FrameEditor>& frames);
      // map with all spritesheets loaded and according animations
      // key: spritesheetname, value: vector of animationsN
      std::map<std::string, std::vector<RyuParser::AnimationEditor> > animations;
      // per spritesheet: frames parsed from the arttool-json, indexed by
      // frame number (needed to create new animations from a frame range)
      std::map<std::string, std::vector<RyuParser::FrameEditor> > parsedFrames;
      bool parsedSpritesheet;
      bool textureSet;
      std::string selectedSpritesheet;
      std::string selectedSpritesheetPath;
      std::string selectedSpritesheetId;
      std::string selectedLevelId;
      char selectedSpriteSheetPath[128] = "assets/spritesheets/ichi/ichi_spritesheet_level1.png";
      EFileBrowserState fileBrowserState;

  private:

      void handleFileBrowser();
      void setTooltipText(const char * tooltip);
      void initTextures();
      void editFrame(RyuParser::AnimationEditor& ani, size_t frame);
      void parseConfiguration(std::string configFile);
      void updateAnimations(RyuParser::JsonAnimations& aniSource);
      void addAnimationsFromJson(std::string spriteSheet, ImportFormat appFormat, json& data);
      GuiCharTextureManager guiCharTextureManager;
      GuiTextureManager guiTextureManager;
      SpritesheetAnimation spritesheetAnimation;

      /*
       * map with animation specifications read from a spritesheet-2D tool (atm: Aseprite-json)
       */
      std::vector<RyuParser::AnimationEditor> animationSpecs;

      std::map<std::string, bool> preferences;

      std::unique_ptr<ImGui::FileBrowser> _fileBrowser;
      bool aniIsPlaying;
  };
}
