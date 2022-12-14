#pragma once

#include "command.h"
#include <map>

class CommandQueue;

namespace sf{
    class Event;
}

class Player
{
    public:
        enum class Action
        {
            MoveLeft,
            MoveRight,
            MoveUp,
            MoveDown
        };

    public:

        Player();

        void assignKey(Action action, sf::Keyboard::Key key);
        sf::Keyboard::Key getAssignedKey(Action action) const;

        void handleEvent(const sf::Event& event,
                         CommandQueue& commands);

        void handleRealtimeInput(CommandQueue& commands);

    private:
        static bool isRealtimeAction(Action action);
        void initializeBindings();

        std::map<sf::Keyboard::Key, Action> mKeyBinding;
        std::map<Action, Command> mActionBinding;

};