#pragma once

#include "Ryu/Control/PlayerController.h"
#include <memory>

class CharacterIchi;

class EventManager {

    public:
       EventManager() {};
       void registerPlayer(std::shared_ptr<CharacterIchi> player);
       std::shared_ptr<CharacterIchi> requestPlayer();

    private:
       std::weak_ptr<CharacterIchi> mPlayableCharacter;
};
