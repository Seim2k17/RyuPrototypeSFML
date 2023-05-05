#include <Ryu/Animation/JsonParser.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <string>
#include <fstream>
#include <iostream>



namespace RyuParser {

    void from_json(const json& j, animation& ani) {
        j.at("Name").get_to(ani.name);
        j.at("Sheet_begin").get_to(ani.fromFrame);
        j.at("Sheet_end").get_to(ani.toFrame);
        j.at("AnimationDirection").get_to(ani.direction);
    }

JsonParser::JsonParser(){}

JsonParser::~JsonParser(){}

void
JsonParser::getAnimationsFromJson(std::string jsonFile)
{
    std::ifstream f(jsonFile);
    fmt::print(stderr, "Open Json, {}!\n", jsonFile);
    try
    {
        json data = json::parse(f);
        fmt::print("Parsing Json from file: \n");  
        std::string jsonString = data.dump();
        try
        {
            fmt::print("JSON-OUTPUT: {}\n",jsonString);
        }
        catch(const std::exception& e)
        {
            fmt::print("Error outputting json-content: {} \n",e.what());            
        }
        // std::cout << jsonString << "\n";
    }
    catch(json::exception e)
    {
      fmt::print("{}: {}\n",e.id,e.what());
      fmt::print("Can't parse file, probably filestream-error. Filename correct ?\n");
    }
}
} /// namespace RyuParser