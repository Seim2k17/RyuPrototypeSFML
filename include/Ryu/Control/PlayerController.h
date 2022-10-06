#pragma once

#include <map>
#include <memory>
#include <SFML/Window/Keyboard.hpp>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Events/Observer.h>

class CommandQueue;
class Command;
class CharacterIchi;

namespace sf{
    class Event;
}

class PlayerController : public Observer
{
    public:

        //PlayerController(std::unique_ptr<CharacterIchi> const &character);
        PlayerController(CharacterIchi* character);

        void assignKey(EInput action, sf::Keyboard::Key key);
        sf::Keyboard::Key getAssignedKey(EInput action) const;
        
        void handleEvent(const sf::Event& event,
                         CommandQueue& commands);

        void handleRealtimeInput(CommandQueue& commands);
        void setActionBindingCharacterSpeed();

        void onNotify(const SceneNode& entity, Event event) override;

    private:
        static bool isRealtimeAction(EInput action);
        void initializeBindings();
        
        std::map<sf::Keyboard::Key, EInput> mKeyBindingPress;
        std::map<EInput, Command> mActionBindingPress;
        std::map<sf::Keyboard::Key, EInput> mKeyBindingRelease;
        std::map<EInput, Command> mActionBindingRelease;

        CharacterIchi* playerCharacter;

};