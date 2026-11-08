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
#include <../build_arch/_deps/tracy-src/public/tracy/Tracy.hpp>

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <iostream>
#include <memory>
#include <string_view>

constexpr std::string_view PROJECT_NAME{"Ryu prototype 0.1"};

// Canvas resolution (640x360)
const sf::Vector2f CANVAS_RESOLUTION{640.f, 360.f};

//namespace ryu{
const sf::Time TimePerFrame = sf::milliseconds(17); // seconds(1.f / 60.f);

Game::Game()
:Observer("Game")
,mWindow(sf::VideoMode({640, 360}), PROJECT_NAME.data()) // 16:9 aspect ratio canvas size
,mEventManager()
,mPlayerController(std::make_unique<PlayerController>(mEventManager))
,mAnimator(RyuAnimator::Editor())
,mIsPaused(false)
,mWorld(mWindow, mEventManager)
,mDebugWidgets(mPlayerController->getPlayableCharacter()) // TODO: player from 1691421player controller
,mDebugWidgetsActive(false)
{
	addObservers();
	mLevelEditor.setWorld(&mWorld);
	//mEventManager.registerPlayer(mPlayerController->getPlayableCharacter());
	// 1. when everthing is loaded and initialized
	// 2. in main.cpp: call run() the game
}

void
Game::useCharacterDebugSettings()
{
			auto player = mPlayerController->getPlayableCharacter();
			// TODO: now in physics set somehow else
/*
			player->setCharacterSettings({
				.MoveMultiplierX = 1.05f,
				.MoveMultiplierY = 1.47f,
				.jumpForwardImpulse = mDebugWidgets.debugData.jumpImpulseForward,
				.jumpUpImpulse = mDebugWidgets.debugData.jumpImpulseUp,
				.massCenter={0,0},
				.bodyMass={18}
			});
*/
			player->setPositionOffset(sf::Vector2f(mDebugWidgets.debugData.positionCross[0],
												   mDebugWidgets.debugData.positionCross[1]));
			player->setOffset(true);
}

void
Game::teleportMainCharacter(float x, float y)
{
	auto player = mPlayerController->getPlayableCharacter();
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
				mDebugWidgetsActive ? useCharacterDebugSettings() : mPlayerController->getPlayableCharacter()->resetCharacterSettings();
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
		case RyuEvent::LevelEditorToggle:
		{
			mLevelEditor.showLevelEditor = not mLevelEditor.showLevelEditor;
			// keep the character visible while editing: scene objects are
			// attached above the player, so the player moves to the top
			// layer and is drawn as a debug single-color fill
			mWorld.setPlayerOnTop(mLevelEditor.showLevelEditor);
			mPlayerController->getPlayableCharacter()->setDebugFillMode(
				mLevelEditor.showLevelEditor);
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
			// mousPos holds WINDOW coordinates captured by ImGui; map them
			// through the active view to world coordinates, otherwise the
			// teleport lands at the raw window position (wrong as soon as
			// window and world sizes differ)
			const sf::Vector2f worldTarget = mWindow.mapPixelToCoords(
				{static_cast<int>(mDebugWidgets.debugData.mousPos[0]),
				 static_cast<int>(mDebugWidgets.debugData.mousPos[1])});
			teleportMainCharacter(worldTarget.x, worldTarget.y);
            fmt::print("Game: Teleport here: {}/{}\n", worldTarget.x, worldTarget.y);
			break;
		}
		case RyuEvent::TemporaryOutput:
		{
			fmt::print("TempOutput\n");
			mPlayerController->getPlayableCharacter()->ouputAnimations();
			break;
		}
		default: break;
	}
}


void
Game::addObservers()
{
	auto player = mPlayerController->getPlayableCharacter();
	player->addObserver(mPlayerController.get());
	player->addObserver(player.get());
	mPlayerController->addObserver(&mWorld);
	mPlayerController->addObserver(this);
	mDebugWidgets.addObserver(/*mWorld.getPlayer()*/this);
}

void Game::run()
{
	sf::Clock clock;
	sf::Clock imguiClock;
	// uses fixed tick steps (use same delta every time)
	sf::Time timeSinceLastUpdate = sf::Time::Zero;
	bool mWindowState = ImGui::SFML::Init(mWindow);
	// TODO: st very weird is going on here:
	// when adjusting the TimePerFrame to a higher fps: the box2d stuff increases but not the
	// EDIT: using a renderer which and only she is, responsibe for rendering should solve this
	// drawing fps of the assets
	// this means: refactor hard: DAMNIT !
	while (mWindow.isOpen() && mWindowState)
	{
		ZoneScopedN("run_Game_while_WindowState");

		timeSinceLastUpdate += clock.restart();
		// Clamp the accumulator after long stalls (window drags, breakpoints, ...)
		// so the game does not try to catch up with a huge burst of updates.
		if (timeSinceLastUpdate > sf::milliseconds(250))
		{
			timeSinceLastUpdate = sf::milliseconds(250);
		}
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
				mPlayerController->handleRealtimeInput(commands);

				update(TimePerFrame*mDebugWidgets.debugData.worldUpdateFactor);
			}
		}

		// ImGui frame: NewFrame, widget calls and Render must happen exactly
		// once per rendered frame (not per simulation tick).
		ImGui::SFML::Update(mWindow, imguiClock.restart());
		if (!mIsPaused)
		{
			mDebugWidgets.CreateDebugGui();
		}//!mAnimator.showAnimationEditor
		mAnimator.createEditorWidgets(&mAnimator.showAnimationEditor);
		mLevelEditor.createEditorWidgets(&mLevelEditor.showLevelEditor);

		// while any editor/overlay is open the game only listens to the
		// mode keys (see PlayerController::handleEvent / handleRealtimeInput)
		mPlayerController->setUiInputActive(
			mDebugWidgetsActive
			|| mAnimator.showAnimationEditor
			|| mLevelEditor.showLevelEditor);

		// Render once per frame, outside the fixed-timestep loop: a slow render
		// must not slow down the simulation.
		  render();

		// Sleep until the next tick instead of busy-spinning at 100% CPU.
		sf::Time idleTime = TimePerFrame - timeSinceLastUpdate;
		if (idleTime > sf::Time::Zero)
		{
			sf::sleep(idleTime);
		}
	}
	ImGui::SFML::Shutdown();
}

void Game::processEvents(std::optional<sf::Event> event, CommandQueue& commands)
{
	ZoneScoped;
		// Player-related one-time events
		// arrow keys navigate the level-editor object list while it is open
		if (mLevelEditor.showLevelEditor && event->is<sf::Event::KeyPressed>())
		{
			const auto* key = event->getIf<sf::Event::KeyPressed>();
			if (key->scancode == sf::Keyboard::Scancode::Up
				|| key->scancode == sf::Keyboard::Scancode::Down
				|| key->scancode == sf::Keyboard::Scancode::Left
				|| key->scancode == sf::Keyboard::Scancode::Right)
			{
				mLevelEditor.handleKey(key->scancode);
			}
		}

		mPlayerController->handleEvent(event.value(), commands);

		// forward game-view mouse events to the level editor while it is open
		if (mLevelEditor.showLevelEditor)
		{
			sf::Vector2i mousePixel;
			if (const auto* moved = event->getIf<sf::Event::MouseMoved>())
			{
				mousePixel = moved->position;
			}
			else if (const auto* mb = event->getIf<sf::Event::MouseButtonPressed>())
			{
				mousePixel = mb->position;
			}
			else if (const auto* mr = event->getIf<sf::Event::MouseButtonReleased>())
			{
				mousePixel = mr->position;
			}

			// forward whenever a mouse position was resolvable (mouse move,
			// button press, button release)
			if (mousePixel != sf::Vector2i{-1, -1})
			{
				mLevelEditor.handleMouseEvent(event.value(),
					mWindow.mapPixelToCoords(mousePixel));
			}
		}
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
	auto player = mPlayerController->getPlayableCharacter();
	mDebugWidgets.debugData.characterState = (player->getCharacterStateEnum())._to_string();
	mDebugWidgets.debugData.characterIsFalling = player->isFalling();
	mDebugWidgets.debugData.numFrames = player->getSpriteAnimation().getNumFrames();
	mDebugWidgets.debugData.numFramesVector = player->getSpriteAnimation().getFramesCount();

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

	//TODO: statistics fps text ....
 	ImGui::SFML::Render(mWindow); /// needs to be called BEFORE display()
	mWindow.display();
}
//} /// namespace ryu
