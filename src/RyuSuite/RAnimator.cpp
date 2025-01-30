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
#include <../build/_deps/tracy-src/public/tracy/Tracy.hpp>

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
                {{"height", ani.frameSize.x}, {"width", ani.frameSize.y}}},
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

// TODO: dynamically initialize array ? -> here elements needs to be iniatilized manually ^^
const char* eventItems[8] = {""};
const char* levelItems[3] = {""};
const char* spritesheetItems[3] = {""};
const char* animationTypes[3] = {""};
const char* animationIds[22] = {""};

// TODO set huge amount of character IDs in the array -> how to improve this ???
// Textures::CharacterID::

static sf::Vector2i sheetPosition{};


void
Editor::initData()
{
    // TODO: use templates and lambdas for initialization !
    int i = 0;
    for (RyuEvent evt : RyuEvent::_values())
    {
        eventItems[i] = evt._to_string();
        ++i;
    }
    // LevelIds for spritesheets
    i = 0;
    for (Textures::LevelID level : Textures::LevelID::_values())
    {
        levelItems[i] = level._to_string();
        ++i;
    }
    // spritesheet ids
    i = 0;
    for (Textures::SpritesheetID spritesheet : Textures::SpritesheetID::_values())
    {
        spritesheetItems[i] = spritesheet._to_string();
        ++i;
    }
    // LevelIds for spritesheets
    i = 0;
    for (Textures::AnimationType aType : Textures::AnimationType::_values())
    {
        animationTypes[i] = aType._to_string();
        ++i;
    }

    i = 0;
    for (Textures::CharacterID cID : Textures::CharacterID::_values())
    {
        animationIds[i] = cID._to_string();
        ++i;
    }
}

void
Editor::update(sf::Time dt)
{
        //fmt::print("update editor");

    if (fileBrowserState != EFileBrowserState::None)
    {
                // mainloop
                //while(continueRendering)
                {
                //...do other stuff like ImGui::NewFrame();
                /*
                if((_fileBrowser != nullptr )&& ImGui::Begin("dummy window"))
                {
                    // open file dialog when user clicks this button
                    if(ImGui::Button("open file dialog"))
                        _fileBrowser->Open();
                }
                ImGui::End();
                */
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

                //...do other stuff like ImGui::Render();
                }
                /*END imbrowser example*/
    }


    if(parsedSpritesheet && aniIsPlaying)
    {
        spritesheetAnimation.update(dt);
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

        // read framespecific data due aseprite-json spec
        if (data.contains("frames"))
        {
            for(auto& ani : animations[selectedSpritesheet])
            {
                for(int i = ani.fromFrame;i<=ani.toFrame;i++)
                {
                    RyuParser::FrameEditor frame;
                    std::string framePosition = selectedSpritesheet+" "+std::to_string(i)+".aseprite";
                    try {
                        frame.duration = data["frames"][framePosition]["duration"];
                        frame.height = data["frames"][framePosition]["frame"]["h"];
                        frame.width = data["frames"][framePosition]["frame"]["w"];
                        frame.x = data["frames"][framePosition]["frame"]["x"];
                        frame.y = data["frames"][framePosition]["frame"]["y"];
                        frame.event = RyuEvent::None;

                    ani.frames.push_back(frame);
                    } catch (std::exception) {
                        fmt::print("Exception thrown: do filenames of .json, .png and .aseprite match: frameposition: {}, spritesheetname: {}?\nse check all filenames.",framePosition, selectedSpritesheet);
                    }
                }
                ani.animationType = Textures::AnimationType::Character;
            }

            setAnimationPreferences(selectedSpritesheet);

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
    
    auto found = path.find_last_of("/\\");

    std::string spriteSheet = path.substr(found+1);  // name of the spritesheet should be st like "ichi_spritesheet_level1");
    auto foundExt = spriteSheet.find_last_of(".json");
    spriteSheet = spriteSheet.substr(0, foundExt-4);
    guiCharTextureManager.load(Textures::LevelID::Level1,"assets/spritesheets/ichi/"+spriteSheet+".png");

    std::ifstream f(path);
    std::cout << "Open JSON...\n";
    try
    {
        json data = json::parse(f);  
        std::cout << "Parsing JSON...\n";
        std::string jsonString = data.dump();
        std::cout << jsonString << "\n";
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
            selectedSpritesheet = spriteSheet;

            addAnimationsFromJson(spriteSheet, appFormat, data);

        }            
        parsedSpritesheet = true;
    }
    catch(json::exception e)
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

        if(ImGui::Begin("Ryu Animation Editor",p_open, window_flags))
        {

          static int selected = 0;

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
          ImGui::BeginChild("left_section",ImVec2(150,0),true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

          int i = 0;
                                
          for(auto& ani : animations[selectedSpritesheet])
          {
            char label[128];
            sprintf(label,"%d_%s", i,ani.name.c_str());
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
                    // intDuration = 0;
                    selectedFrame = 1;
                    // currentEventItem = 0;
                    selectedAnimation = ani;
                    editFrame(ani,selectedFrame);
                }
                catch(std::exception e)
                {
                    fmt::print("Exception thrown when selecting animation {}: {}",ani.name,e.what());
                }
            }
            i++;
          }
      
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
    
    std::cout << "SHEET: " << sheetName << "\n";
    // std::cout << sheetName << ": " << aniName << ": duration " << std::to_string(duration) << "\n";
    for(auto& ani : animations[sheetName])
    {
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

    auto ani = sheet.second.at(selectedAni);
    ImGui::Text((std::to_string(selectedAni)+": "+ani.name+", ").c_str());
    ImGui::SameLine();
    ImGui::Text("Frames : %s, ", (std::to_string(ani.frames.size()).c_str()));
    ImGui::SameLine();
    calculateAnimationDuration(ani);
    ImGui::Text("Ani-Duration: %d ms", ani.animationDuration.asMilliseconds());    
    ImGui::SameLine();
    ImGui::Text("CurrentFrame: %s / %s", std::to_string(currentActiveFrame).c_str(),std::to_string(ani.frames.size()).c_str());
    
    uint16_t frameWidth = ani.frames.at(0).width;
    uint16_t frameHeight = ani.frames.at(0).height;
    
    ImGui::BeginChild("Animation",ImVec2(aniAreaSize.first,aniAreaSize.second),true,ImGuiWindowFlags_NoScrollbar);

    int16_t startX = ani.frames.at(0).x / frameWidth; // for the spritesheetAnimationDetails
    int16_t startY = ani.frames.at(0).y / frameHeight;// for the spritesheetAnimationDetails

    // TODO: here dynamic anis due selection from the right side
    RyuParser::Frame f;
    setSpritesheetAnimationDetails({
                .frameSize={frameWidth,frameHeight}
               ,.startFrame={startX,startY}
               ,.numFrames= ani.numFrames
               ,.duration = ani.animationDuration
               ,.repeat = true ///  TODO from editor ui: entered by user
               ,.animationId = Textures::CharacterID::_from_integral(currentAnimationId)}
        , ani.animationDuration
        ,  ani.frames);///  TODO from editor ui: entered by user
    ImGui::Image(spritesheetAnimation.getSprite()/*guiCharTextureManager.getResource(Textures::LevelID::Level1)*/);
    currentActiveFrame =  spritesheetAnimation.getCurrentFrame()+1;
    // std::cout << currentActiveFrame << " set AniPrefs(dur in ms):" << spritesheetAnimation.getDuration().asMilliseconds() << "\n" ;
    // ImGui::ShowMetricsWindow();
    
    setFrameDetails(selectedAni,sheet,selectedFrame,sheet.second.at(selectedAni));

    // exc start
    ImGui::EndChild();

    
    ImGui::SameLine();

    // TODO: adjust values due Spritesheetsize and FrameSize
    ImGui::BeginChild("SpriteSheet", ImVec2(650,400),true,ImGuiWindowFlags_HorizontalScrollbar ); /// orig. 1040x960

    // sf::Texture* texture = &(guiTextureManager.getResource(Textures::GuiID::FrameBorder));
    ImVec2 pos = ImGui::GetCursorScreenPos();//ImVec2(100,200);//

    // std::cout << "Pos_ori: " << pos.x << "|" << pos.y << "\n";

    try {
    if(currentActiveFrame-1 < ani.frames.size())
    {
        pos.x = pos.x + ani.frames.at(currentActiveFrame-1).x;
        pos.y = pos.y + ani.frames.at(currentActiveFrame-1).y;
    }
    // exception end

    // std::cout << "Pos_new: " << pos.x << "|" << pos.y << "\n";

    // TODO: values according to selectedSpritesheet !
    // ImGui::GetWindowDrawList()->AddImage((void*)texture, pos,ImVec2(pos.x+80,pos.y+96)/* ImVec2(800, 600),ImVec2(880, 696)*/, ImVec2(0,0), ImVec2(1,1),IM_COL32_A_MASK);
    ImGui::GetWindowDrawList()->AddRectFilled(pos,ImVec2(pos.x+80,pos.y+96),IM_COL32(127,0,0,255));//L32_WHITE);

    ImGui::Image(guiCharTextureManager.getResource(Textures::LevelID::Level1));
    // hm how to add image oveer another with SFML?
    
     // std::cout << "whygetcursoscreenpos???\n";
    ImGui::EndChild();
    ImGui::BeginChild("PlayButton");
    // TODO: make nice etc. ImGui needs an id for bnuttons sinc update xyz
    const char *btId = new char('0');

    ImGuiComboFlags flags = 0;
    // TODO us real play/stop etc button .... tmp play button is a arrowbutton

    /*
    ** TODO: rethink the following lines, sinc update bla the ImageButtons doesnt wirk this way
     */
    /*
    //if(ImGui::ArrowButton("Play",ImGuiDir_Right)) {aniIsPlaying = ! aniIsPlaying;}
    if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::StartFrame)))
    {
    }
    setTooltipText();
    ImGui::SameLine();
    if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::BackwardFrame)))
    {}
    setTooltipText();
    ImGui::SameLine();
    if(!aniIsPlaying)
    {
        if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::Play)))
        {
            aniIsPlaying = true;         
        }
    }
    else{
        if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::Stop)))
        {
            aniIsPlaying = false;         
        }
    }
    ImGui::SameLine();
    if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::ForwardFrame)))
    {}
    setTooltipText();
    ImGui::SameLine();
    if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::EndFrame)))
    {
    }
    */
    setTooltipText();

    // show frames as a timeline
    ImGui::BeginChild("TimeLine",ImVec2(frameAreaSize.first,frameAreaSize.second),ImGuiWindowFlags_AlwaysVerticalScrollbar);
        size_t j = 1;
        float currentFrameAreaX = 0;
        for (auto& f : ani.frames)
        {
            //if(ImGui::ImageButton(guiTextureManager.getResource(Textures::GuiID::Frame)))
            if(ImGui::Button(std::to_string(j).c_str(),ImVec2(frameSize.first,frameSize.second)))
            {
                selectedFrame = j;
                editFrame(ani,selectedFrame);
            }
            setTooltipText("click for details");
            if(currentFrameAreaX < frameAreaSize.first)
            {
                currentFrameAreaX += (frameSize.first+10.f);
                //std::cout << currentFrameAreaX << "\n";
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
        // TODO: how to add automatically ?in C-style
        // static char extension[6] = ".json";
        ImGui::InputTextWithHint("Filename","put Json filename here",JsonFilename,IM_ARRAYSIZE(JsonFilename));
        ImGui::InputTextWithHint("Path","put Json filename here",selectedSpriteSheetPath,IM_ARRAYSIZE(selectedSpriteSheetPath));
        if(ImGui::Combo("Spritesheet-Id",&currentSpritesheetItem, spritesheetItems, IM_ARRAYSIZE(spritesheetItems))) //;
        {
            selectedSpritesheetId = Textures::SpritesheetID::_from_integral(currentSpritesheetItem)._to_string();
        }
        if(ImGui::Combo("Level-Id",&currentLevelItem, levelItems, IM_ARRAYSIZE(levelItems))) //;
        {
            selectedLevelId = Textures::LevelID::_from_integral(currentLevelItem)._to_string();
        }
        if(ImGui::Button("Save Json"))
        {
            exportAnimationDetailsToFile(JsonFilename);
        }

     }
     ImGui::EndChild();

    ImGui::EndChild();
    }
    catch(std::exception e)
         {
             fmt::print("createEditorWidgets exception: {}", e.what());
         }
}

void
Editor::setFrameDetails(int selectedAni, TaggedSheetAnimation& sheet, int frameNumber, RyuParser::AnimationEditor& ani)
{
    if(frameDetailsVisible)
    {
        if(ImGui::Combo("AnimationId:", &currentAnimationId, animationIds, IM_ARRAYSIZE(animationIds)))
        {
            ani.animationId = Textures::CharacterID::_from_integral(currentAnimationId);
        }
        ImGui::Separator();
        //auto ani = sheet.second.at(selectedAni);
        ImGui::Text("Frame: %d", frameNumber);
        ImGui::Checkbox("Repeat", &repeatAnimation);
        ani.repeat = repeatAnimation;
        if(ImGui::Combo("AnimationType",&currentAnimationType, animationTypes, IM_ARRAYSIZE(animationTypes))) //;
        {
            ani.animationType = Textures::AnimationType::_from_integral(currentAnimationType);
        }
        if(ImGui::InputInt("Duration",&intDuration))
        {
            if(selectedFrame != 0)
            {
                std::cout << "DurationPress: " << intDuration << "\n";    
                ani.frames.at(frameNumber-1).duration = intDuration ;
            }
        }
        setTooltipText("set duration in ms");
        if(ImGui::Combo("Event",&currentEventItem, eventItems, IM_ARRAYSIZE(eventItems))) //;
        {
            if(selectedFrame != 0)
            {
               std::cout << "Event : " << std::to_string(currentEventItem) << "(" << RyuEvent::_from_integral(currentEventItem)._to_string()  << ") saved to Frame: " << std::to_string(frameNumber) <<" \n";
               //auto ani = sheet.second.at(selectedAni);
               ani.frames.at(frameNumber-1).event = RyuEvent::_from_integral(currentEventItem);
               std::cout << "From ani-map: " << ani.frames.at(frameNumber-1).event._to_string() << "\n";
            }
        }

        //ImGui::Combo("Event",&currentEventItem, EEvent::_names(),3 );
        ImGui::Separator();
    }
}

void
Editor::editFrame(RyuParser::AnimationEditor& ani, size_t frame )
{
    // std::cout << ani.name << "," << std::to_string(frame) << "\n";
    intDuration = ani.frames.at(frame-1).duration;
    currentEventItem = (ani.frames.at(frame-1).event)._to_integral(); 
    // std::cout << "FrameDetail(event): " << currentEventItem << ", ani-map: " << ani.frames.at(frame-1).event._to_string() << "\n";
    //frameDetailsVisible =! frameDetailsVisible;
    ani.frameSize.x = (ani.frames.at(frame-1)).height;
    ani.frameSize.y = (ani.frames.at(frame-1)).width;
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
    // TODO: no filename entered ? what then -> standard name but: HOW 
    std::ofstream oJson(sizeof(JsonFilename) == 0 ? "output.txt" : JsonFilename);
    
    for(auto& ani : animations[selectedSpritesheet])
    {
          aniSpecs.push_back(ani);
    }

    // TODO: use variables for charcter name etc.
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
    std::string pathName(/*selectedSpriteSheetPath); pathName.append(*/JsonFilename);
    fmt::print("saved to {}\n", (sizeof(JsonFilename) == 0 ? "output.txt" : pathName ));

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

} // namespace RyuAnimator
