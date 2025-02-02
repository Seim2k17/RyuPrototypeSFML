#include "Ryu/Statemachine/CharacterState.h"
#include "Ryu/Statemachine/CharacterStateRun.h"
#include <Ryu/Core/Game.h>
#include <Ryu/Core/World.h>
#include <Ryu/Core/AssetManager.h>
#include <Ryu/Core/AssetIdentifiers.h>
#include <Ryu/Character/CharacterBase.h>
#include <Ryu/Character/CharacterIchi.h>
#include <Ryu/Control/PlayerController.h>
#include <Ryu/Control/CharacterEnums.h>
#include <Ryu/Debug/imGuiDebug.h>
#include <RyuSuite/RAnimator.h>

// #include <imgui.h>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <fmt/core.h>
#include <imgui-SFML.h>
// TODO: integrate the files in the project
#include <../build/_deps/tracy-src/public/tracy/Tracy.hpp>

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <string_view>

constexpr std::string_view PROJECT_NAME{"Ryu prototype 0.1"};

//namespace ryu{
const sf::Time TimePerFrame = sf::milliseconds(17); // seconds(1.f / 60.f);

Game::Game()
:Observer("Game")
,mWindow(sf::VideoMode({1200, 800}), PROJECT_NAME.data())
,mPlayerController(std::make_unique<PlayerController>())
,mWorld(mWindow)
,mAnimator(RyuAnimator::Editor())
,mIsPaused(false)
,mDebugWidgets(mWorld.getPlayer()) // TODO: player from 1691421player controller
,mDebugWidgetsActive(false)
{
	// todo: how to load ichis tzextures at  startup ?
	//mPlayer->loadTextures();
	addObservers();
}

void
Game::useCharacterDebugSettings()
{
			auto player = mWorld.getPlayer();
			player->setCharacterSettings({
				.MoveMultiplierX = 1.05f,
				.MoveMultiplierY = 1.47f,
				.jumpForwardImpulse = mDebugWidgets.debugData.jumpImpulseForward,
				.jumpUpImpulse = mDebugWidgets.debugData.jumpImpulseUp,
				.massCenter={0,0},
				.bodyMass={18}
			});

			player->setPositionOffset(sf::Vector2f(mDebugWidgets.debugData.positionCross[0],
												   mDebugWidgets.debugData.positionCross[1]));
			player->setOffset(true);
}

void
Game::teleportMainCharacter(float x, float y)
{
	auto player = mWorld.getPlayer();
	player->teleportCharacter(x, y);
}

void
Game::onNotify(const SceneNode& entity, RyuEvent event)
{
	fmt::print("onNotify in GAME\n");
	switch(event._value)
	{
		case RyuEvent::DebugToggle:
		{
				mDebugWidgets.debugData.activateRyuDebug == false ? mDebugWidgets.debugData.activateRyuDebug = true : mDebugWidgets.debugData.activateRyuDebug = false;
				mDebugWidgetsActive = mDebugWidgets.debugData.activateRyuDebug;
				mDebugWidgetsActive ? useCharacterDebugSettings() : mWorld.getPlayer()->resetCharacterSettings();
				break;
		}
		case RyuEvent::ImGuiDemoToggle:
		{
				mDebugWidgets.debugData.showImGuiDemoWindow == false ? mDebugWidgets.debugData.showImGuiDemoWindow = true : mDebugWidgets.debugData.showImGuiDemoWindow = false;
				break;
		}
		case RyuEvent::RyuAnimatorToggle:
		{
			mAnimator.showAnimationEditor == false ? mAnimator.showAnimationEditor=true : mAnimator.showAnimationEditor=false;
			break;
		}
		case RyuEvent::DebugValuesChanged:
		{
			fmt::print("DebugValues changed, notified in Game\n");
			useCharacterDebugSettings();
			break;
		}
		case RyuEvent::TeleportWait:
		{
			fmt::print("TeleportActive\n");
			break;
		}
		case RyuEvent::TeleportDo:
		{
			fmt::print("TeleportInActive\n");
			teleportMainCharacter(mDebugWidgets.debugData.mousPos[0]
								  ,mDebugWidgets.debugData.mousPos[1]);
            fmt::print("Game: Teleport here: {}/{}\n", mDebugWidgets.debugData.mousPos[0], mDebugWidgets.debugData.mousPos[1]);
			break;
		}
		case RyuEvent::TemporaryOutput:
		{
			fmt::print("TempOutput\n");
			mWorld.getPlayer()->ouputAnimations();
			break;
		}
		default: break;
	}
}


void
Game::addObservers()
{
	auto player = mWorld.getPlayer();
	player->addObserver(mPlayerController.get());
	player->addObserver(player);
	mPlayerController->addObserver(&mWorld);
	mPlayerController->addObserver(this);
	mDebugWidgets.addObserver(/*mWorld.getPlayer()*/this);
}

void Game::run()
{
	sf::Clock clock;
	// uses fixed tick steps (use same delta every time)
	sf::Time timeSinceLastUpdate = sf::Time::Zero;
	bool mWindowState = ImGui::SFML::Init(mWindow);
	// TODO: st very weird is going on here:
	// when adjusting the TimePerFrame to a higher fps: the box2d stuff increases but not the
	// drawing fps of the assets
	// this means: refactor hard: DAMNIT !
	while (mWindow.isOpen() && mWindowState)
	{
		ZoneScopedN("run_Game_while_WindowState");

		timeSinceLastUpdate += clock.restart();
		while (timeSinceLastUpdate > TimePerFrame)
		{
			ZoneScopedN("run_Game_while_timetest");
			timeSinceLastUpdate -= TimePerFrame;
			CommandQueue& commands = mWorld.getActiveCommands();

			while (const std::optional<sf::Event> event = mWindow.pollEvent())
			{
				ZoneScopedN("run_Game_while_poll");
				ImGui::SFML::ProcessEvent(mWindow, event.value());
				processEvents(event.value(), commands);
			}
			if (!mIsPaused )
			{

				ImGui::SFML::Update(mWindow, timeSinceLastUpdate);
				mPlayerController->handleRealtimeInput(commands);

				update(TimePerFrame*mDebugWidgets.debugData.worldUpdateFactor);
				mDebugWidgets.CreateDebugGui();
			}//!mAnimator.showAnimationEditor
			mAnimator.createEditorWidgets(&mAnimator.showAnimationEditor);

		  render();
		}	
	}
	ImGui::SFML::Shutdown();
}

void Game::processEvents(std::optional<sf::Event> event, CommandQueue& commands)
{
	ZoneScoped;
		// Player-related one-time events
		mPlayerController->handleEvent(event.value(), commands);
		//auto evt = event->getIf<T>()


		if (event->is<sf::Event::FocusGained>())
		{
				mIsPaused = false;
		}

		if (event->is<sf::Event::FocusLost>())
		{
			mIsPaused = true;
		}

		if (event->is<sf::Event::Closed>())
		{
			mWindow.close();
		}

}


void
Game::setDebugValues()
{
	mDebugWidgets.debugData.characterState = (mWorld.getPlayer()->getCharacterStateEnum())._to_string();
	mDebugWidgets.debugData.characterIsFalling = mWorld.getPlayer()->isFalling();
	mDebugWidgets.debugData.numFrames = mWorld.getPlayer()->getSpriteAnimation().getNumFrames();
	mDebugWidgets.debugData.numFramesVector = mWorld.getPlayer()->getSpriteAnimation().getFramesCount();

}

void Game::update(sf::Time deltaTime)
{
	ZoneScoped;

	mWorld.update(deltaTime);

	if(mAnimator.fileBrowserState != RyuAnimator::EFileBrowserState::None || (mAnimator.parsedSpritesheet && mAnimator.textureSet))
	{
		mAnimator.update(deltaTime);
	}
	setDebugValues();
}

void Game::render()
{
    ZoneScopedN("Render_Game");

	FrameMark; /// macro to instrumentalize frames for tracy
	mWindow.clear();
	mWorld.draw();

	mWindow.setView(mWindow.getDefaultView());
	//TODO: statistics fps text ....
 	ImGui::SFML::Render(mWindow); /// needs to be called BEFORE display()
	mWindow.display();
}
//} /// namespace ryu
