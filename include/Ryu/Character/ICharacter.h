#pragma once

#include <Ryu/Control/CharacterEnums.h>
#include <SFML/System/Vector2.hpp>

// this is the framesize for the boundary box of the physics body
static constexpr std::pair<int, int> INIT_FRAME_SIZE(60, 86);
static constexpr std::pair<int, int> DUCK_FRAME_SIZE(60, 45);

class ICharacter {
public:
    virtual sf::Vector2f getPosition() = 0;
    virtual ECharacters getCharacterName() = 0;
    virtual ~ICharacter() = default;

};
