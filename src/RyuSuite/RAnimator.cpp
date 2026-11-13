#include "Ryu/Core/AssetIdentifiers.h"
#include <RyuSuite/RAnimator.h>
#include <Ryu/Animation/AnimationData.h>
#include <Ryu/Animation/SpritesheetAnimation.h>
#include <Ryu/Events/EventEnums.h>
#include <Ryu/Animation/EditorEnums.h>
#include <Ryu/Animation/JsonParser.h>

#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <algorithm>
#include <bits/stdint-intn.h>
#include <bits/stdint-uintn.h>
#include <cstddef>
#include <exception>
#include <imgui.h>
#include <imgui-SFML.h>
#include <Thirdparty/imgui-filebrowser/imfilebrowser.h>
#include <nlohmann/json.hpp>
#include <fmt/core.h>
#include <fmt/format.h>

// TODO: integrate the files in the project
#include <tracy/Tracy.hpp>

#include <iostream>
#include <fstream>
#include <string>
#include <variant>
#include <vector>
#include <map>
#include <utility> /// std::pair

//namespace RyuAnimator {
using ImGui::EndTabBar;

using namespace ImGui;
using json = nlohmann::json;
using RyuEvent = Ryu::EEvent;


namespace RyuParser {

    void from_json(const json& j, RyuParser::AnimationEditor& ani) {
        j.at("name").get_to(ani.name);
        j.at("from").get_to(ani.fromFrame);
        j.at("to").get_to(ani.toFrame);
        j.at("direction").get_to(ani.direction);
    }

    void from_json(const json& j, RyuParser::FrameEditor& frame) {
        j.at("frame").at("x").get_to(frame.x);
        j.at("frame").at("y").get_to(frame.y);
        j.at("frame").at("w").get_to(frame.width);
        j.at("frame").at("h").get_to(frame.height);
        j.at("duration").get_to(frame.duration);
    }

    void from_json(const json& j, RyuParser::FrameValues& frameValues) {
        j.at("frame").at("x").get_to(frameValues.x);
        j.at("frame").at("y").get_to(frameValues.y);
        j.at("frame").at("w").get_to(frameValues.width);
        j.at("frame").at("h").get_to(frameValues.height);
    }

    void from_json(const json& j, RyuParser::SpriteSourceSize& sheetValues) {
        j.at("spriteSourceSize").at("x").get_to(sheetValues.x);
        j.at("spriteSourceSize").at("y").get_to(sheetValues.y);
        j.at("spriteSourceSize").at("w").get_to(sheetValues.width);
        j.at("spriteSourceSize").at("h").get_to(sheetValues.height);
    }

    void from_json(const json& j, RyuParser::FrameSize& frameSize) {
        j.at("sourceSize").at("w").get_to(frameSize.width);
        j.at("sourceSize").at("h").get_to(frameSize.height);
    }

    void from_json(const json& j, RyuParser::FramePivot& pivot) {
        j.at("pivot").at("x").get_to(pivot.x);
        j.at("pivot").at("y").get_to(pivot.y);
    }

    void from_json(const json& j, RyuParser::FrameTexturePacker& frame) {
        j.at("filename").get_to(frame.name);
        j.at("frame").at("x").get_to(frame.frame.x);
        j.at("frame").at("y").get_to(frame.frame.y);
        j.at("frame").at("w").get_to(frame.frame.width);
        j.at("frame").at("h").get_to(frame.frame.height);
        j.at("rotated").get_to(frame.rotated);
        j.at("trimmed").get_to(frame.trimmed);
        j.at("spriteSourceSize").at("x").get_to(frame.spriteSourceSize.x);
        j.at("spriteSourceSize").at("y").get_to(frame.spriteSourceSize.y);
        j.at("spriteSourceSize").at("w").get_to(frame.spriteSourceSize.width);
        j.at("spriteSourceSize").at("h").get_to(frame.spriteSourceSize.height);
        j.at("sourceSize").at("w").get_to(frame.sourceSize.width);
        j.at("sourceSize").at("h").get_to(frame.sourceSize.height);
        j.at("pivot").at("x").get_to(frame.pivot.x);
        j.at("pivot").at("y").get_to(frame.pivot.y);
    }

    void to_json(json& j, const RyuParser::FrameEditor& frame){
        // json jEvent = frame.event;
        j = json{
            {"duration",frame.duration},
            {"height",frame.height},
            {"width",frame.width},
            {"x_sheet",frame.x},
            {"y_sheet",frame.y},
            {"event",frame.event._to_string()}
                
        };
    }

    void to_json(json& j, const RyuParser::AnimationEditor& ani) {
      std::string timeInMs{
          std::to_string(ani.animationDuration.asMilliseconds()) + " ms"};
      fmt::print("TO_JSON-anitype: {} \n",ani.animationType._to_string());
      json jFrames = ani.frames;
      j = json{{"Name", ani.name},
               {"Sheet_begin", ani.fromFrame},
               {"Sheet_end", ani.toFrame},
               {"AnimationDirection", ani.direction},
               {"numFrames", ani.numFrames},
               {"Frames", jFrames},
               {"FrameSize",
                {{"height", ani.frameSize.y}, {"width", ani.frameSize.x}}},
               {"animationDuration", timeInMs},
               {"repeat", ani.repeat},
               {"AnimationType", ani.animationType._to_string()},
               {"AnimationId", std::visit( // as animationId is a std::variant with different datatypes we need to use visit
                       [](auto&& cId){return cId._to_string();},
                       ani.animationId)},
               {"PivotNorm",{{"x", ani.pivot.x},{"y", ani.pivot.y} }}
  };
    }

} /// namespace RyuParser


 namespace RyuAnimator{

Editor::Editor():
     parsedSpritesheet(false)
    ,selectedSpritesheet()
    ,showAnimationEditor(false)
    ,guiCharTextureManager()
    ,guiTextureManager()
    ,aniIsPlaying(false)
    ,textureSet(false)
    ,preferences()
    ,fileBrowserState(EFileBrowserState::None)
{
    initTextures();
    initData();
}

Editor::~Editor()
{}

// GUI variables
constexpr std::pair<float,float> frameSize{20.f,35.f}; 
constexpr std::pair<float,float> frameAreaSize{705.f,180.f};
constexpr std::pair<float,float> aniAreaSize{250.f,400.f};
static bool frameDetailsVisible=true;
// content of the selected Frame
/* FrameDetails-Section */
static int intDuration;
static int selectedFrame;
static int currentEventItem = 0;
static int currentLevelItem = 0;
static int currentSpritesheetItem = 0;
// standard is a character animation
static int currentAnimationType = 1;
static int currentAnimationId = 0;
static int currentActiveFrame = 0;
RyuParser::AnimationEditor selectedAnimation;
// standard is a cycled animation
static bool repeatAnimation = true;

// combo items, sized according to the enum values (fixed size arrays could
// overflow when the enums grow)
static std::vector<const char*> eventItems;
static std::vector<const char*> levelItems;
static std::vector<const char*> spritesheetItems;
static std::vector<const char*> animationTypes;
static std::vector<const char*> animationIds;

// TODO set huge amount of character IDs in the array -> how to improve this ???
// Textures::CharacterID::

static sf::Vector2i sheetPosition{};


void
Editor::initData()
{
    eventItems.clear();
    for (RyuEvent evt : RyuEvent::_values())
    {
        eventItems.push_back(evt._to_string());
    }
    // LevelIds for spritesheets
    levelItems.clear();
    for (Textures::LevelID level : Textures::LevelID::_values())
    {
        levelItems.push_back(level._to_string());
    }
    // spritesheet ids
    spritesheetItems.clear();
    for (Textures::SpritesheetID spritesheet : Textures::SpritesheetID::_values())
    {
        spritesheetItems.push_back(spritesheet._to_string());
    }
    // animation types
    animationTypes.clear();
    for (Textures::AnimationType aType : Textures::AnimationType::_values())
    {
        animationTypes.push_back(aType._to_string());
    }

    animationIds.clear();
    for (Textures::CharacterID cID : Textures::CharacterID::_values())
    {
        animationIds.push_back(cID._to_string());
    }
}

void
Editor::update(sf::Time dt)
{
    if(parsedSpritesheet && aniIsPlaying)
    {
        spritesheetAnimation.update(dt);
    }
}

// the filebrowser is an ImGui-popup, so it must be displayed while an
// ImGui-frame is active (i.e. from within createEditorWidgets, not from
// within update() which runs outside the ImGui-frame)
void
Editor::handleFileBrowser()
{
    if (fileBrowserState == EFileBrowserState::None)
    {
        return;
    }

    if(!_fileBrowser)
    {
        fileBrowserState = EFileBrowserState::None;
        return;
    }

    _fileBrowser->Display();

    if(_fileBrowser->HasSelected())
    {
        std::cout << "Selected filename" << _fileBrowser->GetSelected().string() << std::endl;

        switch (fileBrowserState)
        {
            case EFileBrowserState::SpriteSheetJson:
            {
                fmt::print("Path: {}\n", _fileBrowser->GetSelected().string());
                parseJsonData(_fileBrowser->GetSelected().string());
                break;
            }
            case EFileBrowserState::AnimationConfigJson:
            {
                fmt::print("Configure {}\n","Animation");
                parseConfiguration(_fileBrowser->GetSelected().string());
                break;
            }

            default:
                fileBrowserState = EFileBrowserState::None;
        }

        _fileBrowser->ClearSelected();
        fileBrowserState = EFileBrowserState::None;
    }
}

/*
** brief: According the json - format from whch program the json is exported from
** we need different methods of parsing.
** Following json-formats for spritesheets are available
** - Aseprite
** - Texturepacker
*/
void
Editor::addAnimationsFromJson(std::string spriteSheet, ImportFormat appFormat, json& data)
{
    if(appFormat == ImportFormat::Aseprite)
    {
       auto anis = data["meta"]["frameTags"];

       std::vector<RyuParser::AnimationEditor> aniVector;

       for(const auto& a : anis)
       {
         auto ani = a.get<RyuParser::AnimationEditor>();
         aniVector.emplace_back(ani);
       }

       animations.emplace(spriteSheet, aniVector);

       // read framespecific data due aseprite-json spec.
       // the frames of an aseprite-json are keyed "<documentname> <frame>.aseprite",
       // the documentname is the name of the .ase-file (meta:image without
       // extension) and can differ from the name of the exported .json/.png !
       if (data.contains("frames"))
       {
           std::string framePrefix{spriteSheet};
           if (data["meta"].contains("image"))
           {
               std::string image = data["meta"]["image"];
               auto lastDot = image.find_last_of('.');
               if (lastDot != std::string::npos)
               {
                   framePrefix = image.substr(0, lastDot);
               }
           }

           // collect the frames of the sheet, indexed by their frame-number
           std::vector<RyuParser::FrameEditor> sheetFrames;
           for (auto& it : data["frames"].items())
           {
               const std::string& key = it.key();
               auto lastSpace = key.find_last_of(' ');
               if (lastSpace == std::string::npos)
               {
                   continue;
               }
               int frameNumber = -1;
               try
               {
                   frameNumber = std::stoi(key.substr(lastSpace + 1));
               }
               catch(const std::exception&)
               {
                   continue;
               }
               if (frameNumber < 0)
               {
                   continue;
               }

               RyuParser::FrameEditor frame;
               frame.duration = it.value()["duration"];
               frame.height = it.value()["frame"]["h"];
               frame.width = it.value()["frame"]["w"];
               frame.x = it.value()["frame"]["x"];
               frame.y = it.value()["frame"]["y"];
               frame.event = RyuEvent::None;

               if (static_cast<int>(sheetFrames.size()) <= frameNumber)
               {
                   sheetFrames.resize(frameNumber + 1);
               }
               sheetFrames[frameNumber] = frame;
           }
           parsedFrames[spriteSheet] = sheetFrames;

           const auto& frames = parsedFrames[spriteSheet];
           for(auto& ani : animations[spriteSheet])
           {
               for(int i = ani.fromFrame; i <= ani.toFrame; i++)
               {
                   if(i >= 0 && i < static_cast<int>(frames.size())
                      && frames.at(i).width > 0)
                   {
                       ani.frames.push_back(frames.at(i));
                   }
               }
               ani.animationType = Textures::AnimationType::Character;

               if(ani.frames.empty())
               {
                   fmt::print("Animation {} has no frames, check the frametags "
                              "(from {} to {}) against the frames in the json !\n",
                              ani.name, ani.fromFrame, ani.toFrame);
               }
           }

            setAnimationPreferences(spriteSheet);

        }

        std::cout << data["meta"]["frameTags"].dump() << "\n";
       return;
    }

    if(appFormat == ImportFormat::Texturepacker)
    {
        fmt::print("adding animations to animap. \n");

        auto frames = data["frames"];

        std::vector<RyuParser::AnimationEditor> aniVector;
        std::string filename{""};

        std::map<std::string, RyuParser::AnimationEditor> anis{};
        std::set<std::string> aniSet;
        std::string lastAnimation = "";
        int16_t fromFrame, toFrame, posiSheet=0;

        for(const auto& f : frames)
        {
            auto frame = f.get<RyuParser::FrameTexturePacker>();
            filename = frame.name;
            auto lastUnderScore = filename.find_last_of("_");

            filename = filename.substr(0, lastUnderScore);
            aniSet.emplace(filename);

            if(not anis.contains(filename))
            {
                RyuParser::AnimationEditor ani;
                ani.fromFrame = posiSheet;
                ani.name = filename;
                ani.pivot.x = frame.pivot.x;
                ani.pivot.y = frame.pivot.y;
                ani.animationType = Textures::AnimationType::Character;
                anis[filename] = ani;
            }

            RyuParser::FrameEditor eFrame;
            eFrame.duration = 100;
            eFrame.height = frame.frame.height;
            eFrame.width = frame.frame.width;
            eFrame.x = frame.frame.x;
            eFrame.y = frame.frame.y;
            eFrame.event = RyuEvent::None;
            anis.at(filename).frames.push_back(eFrame);
// TODO: we need to save the stuff in global animations map !!!
            ++posiSheet;

            fmt::print("Name: {}\n, Pivot({}/{})\n, Size(h:{},w:{})\n, Frame(h:{},w:{}, x:{}, y:{})\n\n"
                       ,frame.name
                       ,frame.pivot.x
                       ,frame.pivot.y
                       ,frame.sourceSize.height
                       ,frame.sourceSize.width
                       ,frame.frame.height
                       ,frame.frame.width
                       ,frame.frame.x
                       ,frame.frame.y
            );
        }

        for(const auto& a : anis)
        {
            aniVector.emplace_back(a.second);
        }

        animations.emplace(spriteSheet, aniVector);

        bool test = true;

        /*
        {
            auto frame = f.get<RyuParser::FrameTexturePacker>();
            filename = frame.name;
            auto lastUnderScore = filename.find_last_of("_");

            filename = filename.substr(0, lastUnderScore);

            if(filename != lastAnimation)
            {
                RyuParser::AnimationEditor tpAni;
                fromFrame = posiSheet;
                tpAni.fromFrame = fromFrame;
                tpAni.toFrame = toFrame;
                tpAni.numFrames = tpAni.frames.size();

                anis[filename] = tpAni;
            }
            else
            {
                toFrame = posiSheet;
            }

            RyuParser::FrameEditor fr;
            fr.duration = 100;
            fr.height = frame.frame.height;
            fr.width = frame.frame.width;
            fr.x = frame.frame.x;
            fr.y = frame.frame.y;


            anis[filename].frames.emplace_back(fr);

            lastAnimation = filename;


            fmt::print("Add {} to animations. \n", filename);
            // TODO: add frames etc. calculate correctly framepositions with one loop !!!

            ++posiSheet;
        }
    bool test = true; */
    }
    {
      fmt::print("Animation added done \n");
    }
}

/* \brief: At the moment we can only parse jsons generated with Aseprite or Texturepacker,
*          but it could be possible to extend to other PixelArtTools.
*          If you use tags to tag animationse: please make sure that every Animation has exactly one tag.
*          It will be confusing if animations occur in the list which according tags only were created
*          to describe parent states. e.g. "Climbing" as a parent tag, "startClimb", "loopClimb", "endClimb" as childrens.
*          We only would be interested in the child animations !
*/
void
Editor::parseJsonData(std::string path)
{
    fmt::print("Opening {} to read spritesheet.", path);
    // avoid appending animations of multiple spritesheets
    animations.clear();
    parsedFrames.clear();
    preferences.clear();
    
    auto found = path.find_last_of("/\\");

    std::string spriteSheet = path.substr(found+1);  // name of the spritesheet should be st like "ichi_spritesheet_level1");
    auto foundExt = spriteSheet.find_last_of('.');
    if (foundExt != std::string::npos)
    {
        spriteSheet = spriteSheet.substr(0, foundExt);
    }
    // the png sits next to the json (aseprite exports it that way)
    std::string pngPath = path.substr(0, found+1) + spriteSheet + ".png";

    std::ifstream f(path);
    std::cout << "Open JSON...\n";
    try
    {
        json data = json::parse(f);  
        std::cout << "Parsing JSON...\n";
        // build-up metadata
        if (data.contains("meta"))
        {
            ImportFormat appFormat = ImportFormat::None;
            if (data["meta"].contains("app"))
            {
                std::string app = data["meta"]["app"];
                if(app.find("aseprite") != std::string::npos)
                {
                    fmt::print("A aseprite spritesheet\n");
                    appFormat = ImportFormat::Aseprite;

                }
                if(app.find("texturepacker") != std::string::npos)
                {
                    appFormat = ImportFormat::Texturepacker;
                    fmt::print("A texturepacker spritesheet\n");
                }

            }

            // prefer the png which aseprite/texturepacker reference in meta.image
            if (data["meta"].contains("image"))
            {
                std::string image = data["meta"]["image"];
                if (not image.empty())
                {
                    pngPath = path.substr(0, found+1) + image;
                }
            }

            try
            {
                guiCharTextureManager.load(Textures::LevelID::Level1, pngPath);
            }
            catch(const std::exception& e)
            {
                fmt::print("Could not load the spritesheet-image {}: {}\n",
                           pngPath, e.what());
            }

            selectedSpritesheet = spriteSheet;

            addAnimationsFromJson(spriteSheet, appFormat, data);

        }            
        parsedSpritesheet = true;
    }
    catch(const json::exception& e)
    {
        std::cout << e.id << ": " << e.what() << "\n";
        std::cout << "Can't parse file, probably filestream-error. Filename correct ?\n";
    }
    
}

void
Editor::parseConfiguration(std::string configFile)
{
    std::ifstream f(configFile);
    fmt::print(stderr, "Open Json, {}!\n", configFile);
    json data;
    try
    {
        data = json::parse(f);
    }
    catch(json::exception e)
    {
      fmt::print("{}: {}\n",e.id,e.what());
      fmt::print("Can't parse file, probably filestream-error. Filename correct ?\n");
    }

    RyuParser::JsonParser jParser;
    RyuParser::JsonAnimations jsonContent;
    jParser.getAnimationsFromJson(data, jsonContent);
    fmt::print("Parsed Animation configuration: {}\n",jsonContent.jsonName);
    jParser.printJsonParserContent(jsonContent);

    updateAnimations(jsonContent);

}

void
Editor::updateAnimations(RyuParser::JsonAnimations& aniSource)
{
    if(not animations.contains(selectedSpritesheet))
    {
        fmt::print("No spritesheet loaded, open a spritesheet-json first !\n");
        return;
    }
    auto& anims = animations.at(selectedSpritesheet);
    fmt::print("check anis from selected spritesheet: {}\n",selectedSpritesheet);
    // ca this: assert(selectedSpritesheet == aniSource.spritesheetPath);
    if(anims.empty())
    {
        fmt::print("No spritesheet loaded.\n");
    }

    for(auto& aniItem : anims)
    {
        fmt::print("AniName from editor: {}\n",aniItem.name);

        auto predicate = [&aniItem](auto& item){return aniItem.name == item.second.name;};
        auto aniId = std::find_if(aniSource.animations.begin(),aniSource.animations.end(), predicate);
        if(aniId != aniSource.animations.end())
        {
            // set animationId from Config
            aniItem.animationId = aniId->first;

            fmt::print("Ani Id {} set to animation {} \n",
                       std::visit([](auto&& cId){
                          return cId._to_string();},
                           aniItem.animationId), aniItem.name);

            // Frames duration and event
            int i = 0;

            try {
                for (auto& frame : aniId->second.frames)
                {
                    aniItem.frames.at(i).duration = frame.duration;
                    aniItem.frames.at(i).event = frame.event;
                    ++i;
                }
                aniItem.animationDuration = aniId->second.animationDuration;
                aniItem.direction = aniId->second.direction;
                aniItem.repeat = aniId->second.repeat;
            }
            catch(std::exception e)
            {
                fmt::print("Something went wrong here: {} \n",e.what());
            }
        }
    }

    // FIXME: tried set the correct values the current selected animation, but the program crashes here ...
    // editFrame(selectedAnimation, selectedFrame);
}

void
Editor::parseJsonFile()
{

    // TODO: encapsulate this in wrapperclass and set here only which event is thrown (which fileexplorer should open ...)
    // create a file browser instance
    std::unique_ptr<ImGui::FileBrowser> fileDialog;
    fileDialog = std::make_unique<ImGui::FileBrowser>();
    fileDialog->SetTitle("Open Configuration Json");
    fileDialog->SetWindowSize(600, 300);
    fileDialog->SetTypeFilters({ ".json"});

     _fileBrowser = std::move(fileDialog);
     fileBrowserState = EFileBrowserState::AnimationConfigJson;
     _fileBrowser->Open();
}


void
Editor::createEditorWidgets(bool* p_open)
{
    ZoneScopedN("createEdWidgets_RA");

    ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;

    if(showAnimationEditor)
    {
        // make the editor fill most of the game-window on first use
        ImGui::SetNextWindowSize(ImVec2(630, 350), ImGuiCond_FirstUseEver);

        if(ImGui::Begin("Ryu Animation Editor",p_open, window_flags))
        {

          static int selected = 0;

          // the filebrowser-popup must be displayed from within a window
          // (PushID needs a current window), so this cannot be called from
          // within update() which runs outside the ImGui-frame/window
          handleFileBrowser();

          if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
              ImGui::MenuItem("Menu", NULL, false, false);
              if (ImGui::MenuItem("Open Spritesheet JSON (Aseprite) ...")) {
                // TODO: encapsulate this in wrapperclass and set here only which event is thrown (which fileexplorer should open ...)
                // create a file browser instance
                std::unique_ptr<ImGui::FileBrowser> fileDialog;
                fileDialog = std::make_unique<ImGui::FileBrowser>();
                // (optional) set browser properties
                fileDialog->SetTitle("Open Spritesheet Json");
                // the dialog must fit into the (small) game-window
                fileDialog->SetWindowSize(600, 300);
                fileDialog->SetTypeFilters({ ".json"});
                fileBrowserState = EFileBrowserState::SpriteSheetJson;
                _fileBrowser = std::move(fileDialog);

                _fileBrowser->Open();
              }

              if (ImGui::MenuItem("Read Animation Configuration Json ...")){
                parseJsonFile();
              }
                    
              if (ImGui::MenuItem("Close", "Strg+W")) {
                *p_open = false;
              }
              ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
          }

          // Left
          ImGui::BeginChild("left_section",ImVec2(190,0),true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

          int i = 0;

          for(auto& ani : animations[selectedSpritesheet])
          {
            char label[128];
            snprintf(label, sizeof(label), "%d_%s", i,ani.name.c_str());
            if(ImGui::Selectable(label, selected == i)) 
            {
                try
                {
                    fmt::print("choose animation {}\n",ani.name);
                    auto aniId = std::visit( // as animationId is a std::variant with different datatypes we need to use visit
                        [](auto&& cId){return cId._to_integral();},
                        ani.animationId);
                    currentAnimationId = aniId;
                    selected = i;
                    selectedFrame = 1;
                    selectedAnimation = ani;
                    aniIsPlaying = false;
                    editFrame(ani,selectedFrame);
                }
                catch(const std::exception& e)
                {
                    fmt::print("Exception thrown when selecting animation {}: {}",ani.name,e.what());
                }
            }
            i++;
          }

          // create new animations out of the frames of the parsed spritesheet
          ImGui::Separator();
          ImGui::TextUnformatted("Add Animation");
          static char newAniName[64] = "new_animation";
          static int newFromFrame = 0;
          static int newToFrame = 1;
          auto parsed = parsedFrames.find(selectedSpritesheet);
          int numSheetFrames = (parsed != parsedFrames.end())
              ? static_cast<int>(parsed->second.size()) : 0;
          ImGui::InputText("Name##newAni", newAniName, IM_ARRAYSIZE(newAniName));
          ImGui::InputInt("From##newAni", &newFromFrame);
          ImGui::InputInt("To##newAni", &newToFrame);
          ImGui::Text("frames available: %d", numSheetFrames);
          if (ImGui::Button("+ Add Animation"))
          {
              addAnimation(std::string(newAniName), newFromFrame, newToFrame);
              selected = std::max(0,
                      static_cast<int>(animations[selectedSpritesheet].size()) - 1);
          }
          setTooltipText("creates a new animation from the frame-range of "
                         "the parsed spritesheet");
          ImGui::SameLine();
          if (ImGui::Button("- Delete Selected"))
          {
              deleteAnimation(selected);
              selected = 0;
          }
          setTooltipText("deletes the selected animation");

          ImGui::EndChild();
        

        ImGui::SameLine();

        // Right
        // TODO: select spritesheet which is loaded as an tab / select mutlibe Tabs..
        ImGui::BeginGroup();
        if(ImGui::BeginTabBar("SpriteSheets"))
        {
            for (auto& sheet : animations)
            {
                 if(sheet.first == "") continue; /// otherwise we create an empty tab
                 if(ImGui::BeginTabItem(sheet.first.c_str()))
                 {
                     selectedSpritesheet=sheet.first;
                     setAnimationPreferences(sheet.first);
                     createAnimationDetails(selected, sheet);   
                     ImGui::EndTabItem();
                 }
            }

         EndTabBar();
        }
        ImGui::EndGroup();
      }
    ImGui::End();
    }
}

void
Editor::setTooltipText(const char * tooltip="- not implemented -")
{
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip(tooltip);
    }
}

// we need the animation duration before we click on a animation (showing details) bc it can/will be that 
// we instant want to export the data to json -
void
Editor::setAnimationPreferences(std::string sheetName)
{
    if(preferences["animationDurationSet_"+sheetName])
    {
        return;
    }    

    preferences["animationDurationSet_"+sheetName] = true;
    
    for(auto& ani : animations[sheetName])
    {
        if(ani.frames.empty())
        {
            fmt::print("setAnimationPreferences: animation {} of sheet {} has no "
                       "frames, skipping\n", ani.name, sheetName);
            continue;
        }
        size_t i = 0;
        uint16_t durationAni = 0;
        for(const auto& frame : ani.frames)
        {
            i++;
            durationAni+= frame.duration;
        }
        std::cout << "Ani: " << ani.name << ": " << std::to_string(durationAni) << "ms \n";
        ani.animationDuration = sf::milliseconds(durationAni);
        ani.numFrames = i;
        ani.frameSize.y = ani.frames.at(0).height;
        ani.frameSize.x = ani.frames.at(0).width;

        // TODO: set Anidirection / AniId / repeat
    }
    
}

void
Editor::calculateAnimationDuration(RyuParser::AnimationEditor& ani)
{
    int32_t overallDuration = 0;
    for(const auto frame : ani.frames)
    {
        overallDuration+=frame.duration;
    }

    if(intDuration != overallDuration)
    {
        ani.animationDuration = sf::milliseconds(overallDuration);
    }
}


// Note: when editing timing in the animation please note that changing the time in frame 1 leads to a weird overall timing, dont know why :/
void 
Editor::createAnimationDetails(int selectedAni, TaggedSheetAnimation& sheet)
{
    if(selectedAni < 0 || static_cast<size_t>(selectedAni) >= sheet.second.size())
    {
        ImGui::TextUnformatted("No animation selected.");
        return;
    }

    auto& ani = sheet.second.at(selectedAni);
    ImGui::Text((std::to_string(selectedAni)+": "+ani.name+", ").c_str());
    ImGui::SameLine();
    ImGui::Text("Frames : %s, ", (std::to_string(ani.frames.size()).c_str()));
    ImGui::SameLine();
    calculateAnimationDuration(ani);
    ImGui::Text("Ani-Duration: %d ms", ani.animationDuration.asMilliseconds());    
    ImGui::SameLine();
    ImGui::Text("CurrentFrame: %s / %s", std::to_string(currentActiveFrame).c_str(),std::to_string(ani.frames.size()).c_str());

    if(ani.frames.empty())
    {
        ImGui::TextUnformatted("This animation has no frames (nothing parsed "
                               "for its frame-range).");
        return;
    }

    uint16_t frameWidth = ani.frames.at(0).width;
    uint16_t frameHeight = ani.frames.at(0).height;

    // playback controls for the preview
    if(ImGui::Button(aniIsPlaying ? "Pause" : "Play"))
    {
        aniIsPlaying = !aniIsPlaying;
    }
    setTooltipText("play/pause the preview of the animation");
    ImGui::SameLine();
    if(ImGui::Button("Restart"))
    {
        spritesheetAnimation.restart();
    }
    setTooltipText("restart the preview from the first frame");
    ImGui::SameLine();
    ImGui::Checkbox("Repeat", &ani.repeat);

    ImGui::BeginChild("Animation",ImVec2(aniAreaSize.first,aniAreaSize.second),true,ImGuiWindowFlags_NoScrollbar);

    int16_t startX = ani.frames.at(0).x / frameWidth; // for the spritesheetAnimationDetails
    int16_t startY = ani.frames.at(0).y / frameHeight;// for the spritesheetAnimationDetails

    setSpritesheetAnimationDetails({
                .frameSize={frameWidth,frameHeight}
               ,.startFrame={startX,startY}
               ,.numFrames= ani.numFrames
               ,.duration = ani.animationDuration
               ,.repeat = ani.repeat
               ,.animationId = Textures::CharacterID::_from_integral(currentAnimationId)}
        , ani.animationDuration
        ,  ani.frames);
    if(textureSet)
    {
        ImGui::Image(spritesheetAnimation.getSprite());
    }
    currentActiveFrame =  spritesheetAnimation.getCurrentFrame()+1;

    setFrameDetails(selectedAni,sheet,selectedFrame,ani);

    ImGui::EndChild();

    
    ImGui::SameLine();

    // TODO: adjust values due Spritesheetsize and FrameSize
    ImGui::BeginChild("SpriteSheet", ImVec2(650,400),true,ImGuiWindowFlags_HorizontalScrollbar ); /// orig. 1040x960

    ImVec2 pos = ImGui::GetCursorScreenPos();

    if(guiCharTextureManager.isLoaded(Textures::LevelID::Level1))
    {
        const auto& texture = guiCharTextureManager.getResource(Textures::LevelID::Level1);
        ImVec2 texSize(static_cast<float>(texture.getSize().x),
                       static_cast<float>(texture.getSize().y));
        ImGui::Image(texture);

        // highlight the currently displayed frame in the sheet
        size_t highlightFrame = (currentActiveFrame > 0)
            ? static_cast<size_t>(currentActiveFrame - 1) : 0;
        if(highlightFrame < ani.frames.size())
        {
            const auto& frame = ani.frames.at(highlightFrame);
            ImVec2 framePos(pos.x + frame.x, pos.y + frame.y);
            ImVec2 frameEnd(framePos.x + frame.width, framePos.y + frame.height);
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(framePos, frameEnd, IM_COL32(200, 60, 60, 90));
            drawList->AddRect(framePos, frameEnd, IM_COL32(255, 120, 120, 230), 0.f, 0, 2.f);
        }
    }

    ImGui::EndChild();

    // show frames as a timeline
    ImGui::BeginChild("TimeLine",ImVec2(frameAreaSize.first,frameAreaSize.second),ImGuiChildFlags_None,ImGuiWindowFlags_AlwaysVerticalScrollbar);
        size_t j = 1;
        float currentFrameAreaX = 0;
        for (auto& f : ani.frames)
        {
            // frames which carry an event get an asterisk
            std::string label = std::to_string(j);
            if(f.event != +RyuEvent::None)
            {
                label += "*";
            }
            if(selectedFrame == static_cast<int>(j))
            {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.26f, 0.59f, 0.98f, 0.80f));
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
            }
            if(ImGui::Button(label.c_str(),ImVec2(frameSize.first,frameSize.second)))
            {
                selectedFrame = j;
                editFrame(ani,selectedFrame);
            }
            if(selectedFrame == static_cast<int>(j))
            {
                ImGui::PopStyleColor(2);
            }
            setTooltipText("click for details, * = frame has an event");
            if(currentFrameAreaX < frameAreaSize.first)
            {
                currentFrameAreaX += (frameSize.first+10.f);
                ImGui::SameLine();
            }
            else
            {
                currentFrameAreaX = 0;
            }
            ++j;
        }
    ImGui::EndChild();
    
    // Save Json-Data
    ImGui::SameLine();
    if(ImGui::BeginChild("Preferences", ImVec2(0,0), false))
    {
        // to use with std::string or own datatype we need a wrapper for InputTextXXX)
        static char JsonFilename[128] = "jsonFile.json";
        ImGui::InputTextWithHint("Filename","put Json filename here",JsonFilename,IM_ARRAYSIZE(JsonFilename));
        ImGui::InputTextWithHint("Path","put spritesheet path here",selectedSpriteSheetPath,IM_ARRAYSIZE(selectedSpriteSheetPath));
        if(ImGui::Combo("Spritesheet-Id",&currentSpritesheetItem, spritesheetItems.data(), static_cast<int>(spritesheetItems.size())))
        {
            selectedSpritesheetId = Textures::SpritesheetID::_from_integral(currentSpritesheetItem)._to_string();
        }
        if(ImGui::Combo("Level-Id",&currentLevelItem, levelItems.data(), static_cast<int>(levelItems.size())))
        {
            selectedLevelId = Textures::LevelID::_from_integral(currentLevelItem)._to_string();
        }
        if(ImGui::Button("Save Json"))
        {
            exportAnimationDetailsToFile(JsonFilename);
        }

     }
     ImGui::EndChild();
}

void
Editor::setFrameDetails(int selectedAni, TaggedSheetAnimation& sheet, int frameNumber, RyuParser::AnimationEditor& ani)
{
    if(frameDetailsVisible)
    {
        if(ImGui::Combo("AnimationId:", &currentAnimationId, animationIds.data(), static_cast<int>(animationIds.size())))
        {
            ani.animationId = Textures::CharacterID::_from_integral(currentAnimationId);
        }
        ImGui::Separator();
        //auto ani = sheet.second.at(selectedAni);
        ImGui::Text("Frame: %d", frameNumber);
        ImGui::Checkbox("Repeat", &repeatAnimation);
        ani.repeat = repeatAnimation;
        if(ImGui::Combo("AnimationType",&currentAnimationType, animationTypes.data(), static_cast<int>(animationTypes.size())))
        {
            ani.animationType = Textures::AnimationType::_from_integral(currentAnimationType);
        }
        if(ImGui::InputInt("Duration",&intDuration))
        {
            if(frameNumber > 0 && static_cast<size_t>(frameNumber) <= ani.frames.size())
            {
                ani.frames.at(frameNumber-1).duration = intDuration ;
            }
        }
        setTooltipText("set duration in ms");
        if(ImGui::Combo("Event",&currentEventItem, eventItems.data(), static_cast<int>(eventItems.size())))
        {
            if(frameNumber > 0 && static_cast<size_t>(frameNumber) <= ani.frames.size())
            {
               ani.frames.at(frameNumber-1).event = RyuEvent::_from_integral(currentEventItem);
               std::cout << "Event " << RyuEvent::_from_integral(currentEventItem)._to_string()
                         << " saved to Frame: " << std::to_string(frameNumber) << "\n";
            }
        }

        //ImGui::Combo("Event",&currentEventItem, EEvent::_names(),3 );
        ImGui::Separator();
    }
}

void
Editor::editFrame(RyuParser::AnimationEditor& ani, size_t frame )
{
    if(frame == 0 || frame > ani.frames.size())
    {
        return;
    }
    intDuration = ani.frames.at(frame-1).duration;
    currentEventItem = (ani.frames.at(frame-1).event)._to_integral();
    // frameSize.x holds the width, frameSize.y the height
    ani.frameSize.x = (ani.frames.at(frame-1)).width;
    ani.frameSize.y = (ani.frames.at(frame-1)).height;
    repeatAnimation = ani.repeat;
}

void
Editor::initTextures()
{
    // TODO: still valid ?: this could also be done when opeing the spritesheet through a dialog ... whe the size of the spritesheets become bigger this will be a memory killer
    //guiCharTextureManager.load(Textures::LevelID::Level1,"assets/spritesheets/ichi/ichi_spritesheet_level1.png");
    guiTextureManager.load(Textures::GuiID::ForwardFrame,"assets/gui/animator/06_nextFrame.jpeg");
    guiTextureManager.load(Textures::GuiID::BackwardFrame,"assets/gui/animator/03_previousFrame.jpeg");
    guiTextureManager.load(Textures::GuiID::Play,"assets/gui/animator/04_playAni.jpeg");
    guiTextureManager.load(Textures::GuiID::Stop,"assets/gui/animator/05_stopAni.jpeg");
    guiTextureManager.load(Textures::GuiID::StartFrame,"assets/gui/animator/02_startFrame.jpeg");
    guiTextureManager.load(Textures::GuiID::EndFrame,"assets/gui/animator/07_lastFrame.jpeg");
    guiTextureManager.load(Textures::GuiID::Frame,"assets/gui/animator/01_frame.jpeg");
    guiTextureManager.load(Textures::GuiID::FrameBorder,"assets/gui/animator/08_border_frame.jpeg");

    
}

void
Editor::setSpritesheetAnimationDetails(const AnimationConfig& config, sf::Time aniDuration, std::vector<RyuParser::FrameEditor>& frames)
{   
    // std::cout << "OverallDuration: " << aniDuration.asMilliseconds() << " ms, config-dur: " << config.duration.asMilliseconds() << "ms\n";
    spritesheetAnimation.setFrameSize(config.frameSize);
    spritesheetAnimation.setStartFrame({config.frameSize.x * config.startFrame.x, config.frameSize.y * config.startFrame.y});
    spritesheetAnimation.setNumFrames(aniDuration, frames);
    spritesheetAnimation.setDuration(config.duration);
    spritesheetAnimation.setRepeating(config.repeat);

    // TODO: check if texture is set for animation ...
    spritesheetAnimation.setTexture(guiCharTextureManager.getResource(Textures::LevelID::Level1/*config.spritesheetId*/));
    textureSet = true;
    // set origin of texture to center
    sf::FloatRect bounds = spritesheetAnimation.getSprite().getLocalBounds();
    spritesheetAnimation.getSprite().setOrigin(bounds.getCenter());
 
}

void
Editor::exportAnimationDetailsToFile(char* JsonFilename)
{
    std::vector<RyuParser::AnimationEditor> aniSpecs;
    std::string filename{JsonFilename != nullptr ? JsonFilename : ""};
    if(filename.empty())
    {
        filename = "output.json";
    }
    std::ofstream oJson(filename);
    if(not oJson.is_open())
    {
        fmt::print("Could not open {} for writing !\n", filename);
        return;
    }

    for(auto& ani : animations[selectedSpritesheet])
    {
          aniSpecs.push_back(ani);
    }

    oJson << "{\n" << R"(  "Name" : "ichi",)" << "\n";
    oJson << R"(  "Spritesheet" : ")" << selectedSpritesheetId << R"(",)" << "\n";
    oJson << R"(  "Level" : ")" << selectedLevelId << R"(",)" << "\n";
    oJson << R"(  "Path" : ")" << selectedSpriteSheetPath << R"(",)" << "\n";
    oJson << R"(  "Animations" : [)" << "\n    ";

    int lineNr = 0;
    for(const auto& spec : aniSpecs)
    {
        lineNr++;
        json j = spec;
        oJson << j;
        if (lineNr != aniSpecs.size())
        {
            oJson << R"(,)";
        }
        oJson << "\n    " ;
    }

    oJson << R"(])" << "\n}";
    fmt::print("saved {} animation(s) to {}\n", aniSpecs.size(), filename);

    // json j()
    /* 
    Output json: example ichi.json
    {
        "Name" : "Ichi",
        "Spritesheet" : "level1.png"
        "Animations" :
        [
            {"name": "idle", 
            "sheetStart" : {"x":1, "y":1}, 
            "frameSize" : {"width" : 80, "height" : 96},
            "numFrames" : 4,
            "duration_ms" : 800,
            "frames" : 
                [
                    {"number" : 1, "duration_ms" : 200, "event" : "None"},
                    {"number" : 2, "duration_ms" : 120, "event" : "None"},
                    {"number" : 3, "duration_ms" : 80, "event" : "None"},
                    {"number" : 4, "duration_ms" : 400, "event" : "FootstepSFX"},
                ],
            "repeat" : true,
            "animationId" : "Textures::CharacterID::IchiIdleRun"        
            },
        ]
    }
    // built asserts if total duration = sumFrames 
    */
    // atm we need to save:
    /*
   .frameSize={frameWidth,frameHeight}
   .startFrame={startX,startY} // reg. the spritesheet
   .numFrames=i
   .duration = sf::milliseconds(durationAni)// in ms
   .repeat = true ///  TODO from editor ui: entered by user
   .animationId = Textures::CharacterID::IchiIdleRun});///  TODO from editor ui: entered by user    
   ^*/
}

// creates a new animation out of the frames of the parsed spritesheet,
// the framenumbering is the one of the arttool (aseprite)
void
Editor::addAnimation(const std::string& name, int fromFrame, int toFrame)
{
    auto parsed = parsedFrames.find(selectedSpritesheet);
    if(parsed == parsedFrames.end() || parsed->second.empty())
    {
        fmt::print("addAnimation: no parsed spritesheet-frames available, "
                   "open a spritesheet-json first !\n");
        return;
    }
    const auto& sheetFrames = parsed->second;

    if(name.empty())
    {
        fmt::print("addAnimation: please enter a name for the animation !\n");
        return;
    }
    if(fromFrame < 0 || toFrame < fromFrame
       || toFrame >= static_cast<int>(sheetFrames.size()))
    {
        fmt::print("addAnimation: invalid framerange {} - {}, the parsed "
                   "spritesheet has frames 0 - {}\n",
                   fromFrame, toFrame, static_cast<int>(sheetFrames.size()) - 1);
        return;
    }

    RyuParser::AnimationEditor ani;
    ani.name = name;
    ani.fromFrame = fromFrame;
    ani.toFrame = toFrame;
    ani.direction = "forward";
    ani.animationType = Textures::AnimationType::Character;
    ani.repeat = false;
    for(int i = fromFrame; i <= toFrame; i++)
    {
        if(sheetFrames.at(i).width > 0)
        {
            ani.frames.push_back(sheetFrames.at(i));
        }
    }

    if(ani.frames.empty())
    {
        fmt::print("addAnimation: no frames found in range {} - {}\n",
                   fromFrame, toFrame);
        return;
    }

    uint16_t duration = 0;
    for(const auto& frame : ani.frames)
    {
        duration += frame.duration;
    }
    ani.numFrames = ani.frames.size();
    ani.animationDuration = sf::milliseconds(duration);
    ani.frameSize.x = ani.frames.at(0).width;
    ani.frameSize.y = ani.frames.at(0).height;

    animations[selectedSpritesheet].push_back(ani);
    fmt::print("addAnimation: added {} (frames {} - {}, {} frames, {} ms)\n",
               ani.name, fromFrame, toFrame, ani.numFrames, duration);
}

void
Editor::deleteAnimation(int index)
{
    auto& anis = animations[selectedSpritesheet];
    if(index < 0 || static_cast<size_t>(index) >= anis.size())
    {
        return;
    }
    fmt::print("deleteAnimation: removed {}\n", anis.at(index).name);
    anis.erase(anis.begin() + index);
    selectedFrame = 1;
    aniIsPlaying = false;
    spritesheetAnimation.restart();
}

} // namespace RyuAnimator
