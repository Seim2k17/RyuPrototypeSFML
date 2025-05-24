#pragma once

#include <SFML/System/Time.hpp>
#include <map>
#include <memory>
#include <SFML/Window/Keyboard.hpp>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Events/Observer.h>
#include <Ryu/Events/Subject.h>
#include <Ryu/Events/EventEnums.h>


class CommandQueue;
class Command;
class CharacterIchi;
class Time;

using RyuEvent = Ryu::EEvent;

namespace sf{
    class Event;
}

class PlayerController : public Observer, public Subject /// notifier and observer at the same time
{
    public:

        //PlayerController(std::unique_ptr<CharacterIchi> const &character);
        PlayerController();

        void assignKey(EInput action, sf::Keyboard::Key key);
        sf::Keyboard::Key getAssignedKey(EInput action) const;
        const std::shared_ptr<CharacterIchi> getPlayableCharacter();
        
        void handleEvent(const sf::Event& event,
                         CommandQueue& commands);

        void handleRealtimeInput(CommandQueue& commands);
        void setActionBindingCharacterSpeed();
        void update(sf::Time deltatime);

        void onNotify(const SceneNode& entity, RyuEvent event) override;

    private:
        static bool isRealtimeAction(EInput action);
        void initializeBindings();
        
        std::map<sf::Keyboard::Key, EInput> mKeyBindingPress;
        std::map<EInput, Command> mActionBindingPress;
        std::map<sf::Keyboard::Key, EInput> mKeyBindingRelease;
        std::map<EInput, Command> mActionBindingRelease;

        std::shared_ptr<CharacterIchi> playerCharacter;

};
