#pragma once

#include <Ryu/Control/CharacterEnums.h>
#include <SFML/System/Vector2.hpp>

// this is the framesize for the boundary box of the physics body
// (old sheet: 80x96 frames -> 60x86 box; 64x64 sheet keeps proportionally
// more slack around the ~28x53 sprite content, duck box covers the content
// below the frame center, ~29px + slack)
static constexpr std::pair<int, int> INIT_FRAME_SIZE(44, 58);
static constexpr std::pair<int, int> DUCK_FRAME_SIZE(44, 34);

class ICharacter {
public:
    virtual sf::Vector2f getPosition() = 0;
    virtual void setBodyId(b2BodyId bodyId) = 0;
    virtual ECharacters getCharacterName() = 0;
    virtual ~ICharacter() = default;

};
