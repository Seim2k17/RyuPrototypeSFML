
#include <Ryu/Control/PlayerController.h>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Events/EventEnums.h>

//#include "aircarft.h"
#include <Ryu/Core/Category.h>
#include <Ryu/Core/Command.h>
#include <Ryu/Core/CommandQueue.h>
#include <Ryu/Character/CharacterIchi.h>

#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <fmt/core.h>
#include <iostream>
#include <map>
#include <memory>
#include <tracy/Tracy.hpp>

constexpr sf::Vector2i CHAR_START_POSITION{150,200};

// command-actions
struct CharacterMover
{
	CharacterMover(float vx, float vy)
	: velocity(vx, vy)
	{
	}

    // execute CharacterMover
	void operator() (CharacterIchi& character, sf::Time) const
	{
       //fmt::print("MoveCharacter\n");
       character.moveCharacter(velocity);
	}

	sf::Vector2f velocity;
};


// small adapter that takes a function on a derived class and converts it to a function on the SceneNode base class
template <typename GameObject, typename Function>
std::function<void(SceneNode&, sf::Time)> derivedEInput(Function fn)
{
    return [=](SceneNode& node, sf::Time dt)
    {
        // check if cast is safe (debug-mode)
        assert(dynamic_cast<GameObject*>(&node) != nullptr);

        // downcast node and invoke function on it
        fn(static_cast<GameObject&>(node),dt);
    };
}

PlayerController::PlayerController()
: Observer("Playercontroller")
  // TODO: initialize other member
, playerCharacter(std::make_shared<CharacterIchi>(ECharacterState::Idle, CHAR_START_POSITION))
{
    initializeBindings();
}

const std::shared_ptr<CharacterIchi>
PlayerController::getPlayableCharacter()
{
    return playerCharacter;
}

void
PlayerController::onNotify(const SceneNode& entity, RyuEvent event)
{
    // fmt::print("received change for characterspeed");
    switch (event._value)
    {
        case RyuEvent::CharacterSpeedChanged:
        {
            setActionBindingCharacterSpeed();
            break; 
        }
            
        default:
            break;
        }
}

void
PlayerController::update(sf::Time deltaTime)
{
    playerCharacter->update(deltaTime);
}

void
PlayerController::setActionBindingCharacterSpeed()
{
    // TODO: check if this is also called correctly if we change the characterspeed
    float characterSpeed = playerCharacter->getCharacterSpeed();

    mActionBindingPress[EInput::PressLeft].action = derivedEInput<CharacterIchi>(
            CharacterMover(-characterSpeed,0.f));
    mActionBindingPress[EInput::PressRight].action = derivedEInput<CharacterIchi>(
            CharacterMover(characterSpeed,0.f));
    mActionBindingPress[EInput::PressUp].action = derivedEInput<CharacterIchi>(
            CharacterMover(0.f,-characterSpeed));
            //CharacterMover(0.f,0.f));
    mActionBindingPress[EInput::PressDown].action = derivedEInput<CharacterIchi>(
            CharacterMover(0.f,characterSpeed));
}

void
PlayerController::initializeBindings()
{
    mKeyBindingPress[sf::Keyboard::Key::Left] = EInput::PressLeft;
    mKeyBindingPress[sf::Keyboard::Key::A] = EInput::PressLeft;
    mKeyBindingPress[sf::Keyboard::Key::Right] = EInput::PressRight;
    mKeyBindingPress[sf::Keyboard::Key::D] = EInput::PressRight;
    mKeyBindingPress[sf::Keyboard::Key::Up] = EInput::PressUp;
    mKeyBindingPress[sf::Keyboard::Key::W] = EInput::PressUp;
    mKeyBindingPress[sf::Keyboard::Key::Down] = EInput::PressDown;
    mKeyBindingPress[sf::Keyboard::Key::S] = EInput::PressDown;

    mKeyBindingRelease[sf::Keyboard::Key::Left] = EInput::ReleaseLeft;
    mKeyBindingRelease[sf::Keyboard::Key::A] = EInput::ReleaseLeft;
    mKeyBindingRelease[sf::Keyboard::Key::Right] = EInput::ReleaseRight;
    mKeyBindingRelease[sf::Keyboard::Key::D] = EInput::ReleaseRight;
    mKeyBindingRelease[sf::Keyboard::Key::Up] = EInput::ReleaseUp;
    mKeyBindingRelease[sf::Keyboard::Key::W] = EInput::ReleaseUp;
    mKeyBindingRelease[sf::Keyboard::Key::Down] = EInput::ReleaseDown;
    mKeyBindingRelease[sf::Keyboard::Key::S] = EInput::ReleaseDown;

    setActionBindingCharacterSpeed();

    mActionBindingRelease[EInput::ReleaseLeft].action = derivedEInput<CharacterIchi>(
            CharacterMover(0.f,0.f));
    mActionBindingRelease[EInput::ReleaseRight].action = derivedEInput<CharacterIchi>(
            CharacterMover(0.f,0.f));
    mActionBindingRelease[EInput::ReleaseUp].action = derivedEInput<CharacterIchi>(
            CharacterMover(0.f,0.f));
    mActionBindingRelease[EInput::ReleaseDown].action = derivedEInput<CharacterIchi>(
            CharacterMover(0.f,0.f));

    for(auto& actionBinding : mActionBindingPress,mActionBindingRelease)
    {
        actionBinding.second.category = static_cast<unsigned>(Category::Type::Player); 
    }
}

void
PlayerController::handleEvent(const sf::Event& event, CommandQueue& commands)
{
   /* 
   * TODO
   * use a binding map from EInput to functioncall
   * 
   */

    //std::cout << "PlayerHandleEvent " << event.type << std::endl;
    // first test for one-time events
    if(const auto* keyPressed = event.getIf<sf::Event::KeyPressed>())
    {
        switch (keyPressed->scancode)
        {
            case sf::Keyboard::Scancode::D:
            case sf::Keyboard::Scancode::Right:
            {
                playerCharacter->handleInput(EInput::PressRight);
                playerCharacter->getSpriteAnimation().flipAnimationRight();
                playerCharacter->setMoveDirection(EMoveDirection::Right);
                break;
            }

            case sf::Keyboard::Scancode::A:
            case sf::Keyboard::Scancode::Left:
            {
                playerCharacter->handleInput(EInput::PressLeft);
                playerCharacter->getSpriteAnimation().flipAnimationLeft();
                playerCharacter->setMoveDirection(EMoveDirection::Left);
                break;
            }

            case sf::Keyboard::Scancode::W:
            case sf::Keyboard::Scancode::Up:
            {
                playerCharacter->handleInput(EInput::PressUp);
                break;
            }

            case sf::Keyboard::Scancode::S:
            case sf::Keyboard::Scancode::Down:
            {
                //std::cout << "Left pressed " << std::endl;
                playerCharacter->handleInput(EInput::PressDown);
                break;
            }
            case sf::Keyboard::Scancode::E:
            {
                //std::cout << "Left pressed " << std::endl;
                playerCharacter->jumpForward();
                break;
            }
            // TODO: introduce debug trigger
            // Debug Area
            case sf::Keyboard::Scancode::P:
            {
                Command output;
                output.category = static_cast<unsigned>(Category::Type::Player);
                output.action = [](SceneNode& s,sf::Time)
                {
                    std::cout << s.getPosition().x << "," << s.getPosition().y << "\n";
                };
                notify(*playerCharacter,RyuEvent::DebugToggle);
                break;
            }
            case sf::Keyboard::Scancode::F1:
            {
                notify(*playerCharacter,RyuEvent::ImGuiDemoToggle);
                break;
            }
            case sf::Keyboard::Scancode::F2:
            {
                 notify(*playerCharacter,RyuEvent::RyuAnimatorToggle);
                 break;
            }

            case sf::Keyboard::Scancode::O:
            {
                std::cout << "Scale(" << playerCharacter->getSpriteAnimation().getSprite().getScale().x << "," 
                << playerCharacter->getSpriteAnimation().getSprite().getScale().y <<  ")\n"; //.scale({1,1});
                std::cout << "MoveDir: " << (int)playerCharacter->getMoveDirection() << "\n";
                notify(*playerCharacter,RyuEvent::TemporaryOutput);

                break;
            }
            case sf::Keyboard::Scancode::R:
            {
                playerCharacter->toggleTestStates();;
                break;
            }
            case sf::Keyboard::Scancode::U:
            {
                playerCharacter->getSpriteAnimation().getSprite().setScale({-1,1});
                break;
            }
            case sf::Keyboard::Scancode::I:
            {
                playerCharacter->getSpriteAnimation().getSprite().setScale({1,1});
                break;
            }

            default:
                break;
        }
    }
    if(const auto * keyReleased = event.getIf<sf::Event::KeyReleased>())
    {
        switch (keyReleased->scancode)
        {
            case sf::Keyboard::Scancode::D:
            case sf::Keyboard::Scancode::Right:
            {
                playerCharacter->handleInput(EInput::ReleaseRight);
                commands = {};
                commands.push(mActionBindingRelease[EInput::ReleaseRight]);
                break;
            }
            case sf::Keyboard::Scancode::A:
            case sf::Keyboard::Scancode::Left:
            {
                playerCharacter->handleInput(EInput::ReleaseLeft);
                /* TODO: make this less boilerplate ! 
                here we need to clear all pending movement-commands 
                -> is this a problem for later commands ? maybe only remove category commands
                */
                commands = {};
                commands.push(mActionBindingRelease[EInput::ReleaseLeft]);
                break;
            }

            case sf::Keyboard::Scancode::S:
            case sf::Keyboard::Scancode::Down:
            {
                playerCharacter->handleInput(EInput::ReleaseDown);
                commands = {};
                commands.push(mActionBindingRelease[EInput::ReleaseDown]);
                break;
            }

            case sf::Keyboard::Scancode::W:
            case sf::Keyboard::Scancode::Up:
            {
                playerCharacter->handleInput(EInput::ReleaseUp);
                commands = {};
                commands.push(mActionBindingRelease[EInput::ReleaseUp]);
                break;
            }

            default:
                break;
        }
    }
}

void
PlayerController::handleRealtimeInput(CommandQueue& commands)
{
    ZoneScopedN("handleInput_PlCtrl");

    for(auto const& binding : mKeyBindingPress)
    {
        if(sf::Keyboard::isKeyPressed(binding.first) && isRealtimeAction(binding.second))
        {
            commands.push(mActionBindingPress[binding.second]);
        }
    }    
}

bool
PlayerController::isRealtimeAction(EInput action)
{
	switch (action)
	{
		case EInput::PressLeft:
		case EInput::PressRight:
		case EInput::PressDown:
		case EInput::PressUp:
			return true;

		default:
			return false;
	}
}

void
PlayerController::assignKey(EInput action, sf::Keyboard::Key key)
{
	// Remove all keys that already map to action
	for (auto itr = mKeyBindingPress.begin(); itr != mKeyBindingPress.end(); )
	{
		if (itr->second == action)
			mKeyBindingPress.erase(itr++);
		else
			++itr;
	}

	// Insert new binding
	mKeyBindingPress[key] = action;
}

sf::Keyboard::Key
PlayerController::getAssignedKey(EInput action) const
{
    for(auto& binding : mKeyBindingPress)
    {
        if (binding.second == action)
			return binding.first;
    }
	
	return sf::Keyboard::Key::Unknown;
}
