#include "Ryu/Control/PlayerController.h"
#include <Ryu/Core/EventManager.h>

void
EventManager::registerPlayer(std::shared_ptr<CharacterIchi> player)
{
   mPlayableCharacter = player;
}

std::shared_ptr<CharacterIchi>
EventManager::requestPlayer()
{
    return mPlayableCharacter.lock();
}
