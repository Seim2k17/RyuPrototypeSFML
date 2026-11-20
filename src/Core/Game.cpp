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
#include <nlohmann/json.hpp>
// TODO: integrate the files in the project
#include <tracy/Tracy.hpp>

#include <SFML/Graphics.hpp>
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <memory>
#include <string_view>

#include <unistd.h>

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
,mDebugWidgets(mPlayerController->getPlayableCharacter(), mWindow) // TODO: player from 1691421player controller
,mDebugWidgetsActive(false)
{
	setupLogCapture();
	addObservers();
	mLevelEditor.setWorld(&mWorld);
	// "Save as default" in the level editor then persists the whole editor
	// layout: play window size/pos, zoom and all ImGui window positions
	mLevelEditor.setLayoutSaveHook([this]() { saveEditorLayout(); });
	// restore the last editor layout: play window size/position and, if
	// the game was left in editing state, the open editors (and zoom)
	loadEditorLayout();
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
			// keep the character visible while editing: scene objects are
			// attached above the player, so the player moves to the top
			// layer and is drawn as a debug single-color fill
			setLevelEditorOpen(not mLevelEditor.showLevelEditor);
			break;
		}
		case RyuEvent::ScriptEditorToggle:
		{
			mScriptEditor.showScriptEditor = not mScriptEditor.showScriptEditor;
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
	// deterministic ImGui layout persistence: disable the default
	// imgui.ini handling and load the stored window positions/sizes from
	// a fixed path next to the other editor layout settings
	if (mWindowState)
	{
		ImGui::GetIO().IniFilename = nullptr;
		std::ifstream iImGuiLayout(mImGuiLayoutPath);
		if (iImGuiLayout.is_open())
		{
			ImGui::LoadIniSettingsFromDisk(mImGuiLayoutPath.c_str());
			fmt::print("Game: loaded ImGui layout from {}\n",
					   mImGuiLayoutPath);
		}
	}
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

		// switch the window mode right before the ImGui frame: editor keys
		// (F3 etc.) are processed above, so applying the mode here means the
		// editor window and its geometry exist BEFORE any editor widget
		// docks itself (first-use docking would otherwise still see the
		// small play window and dock panels inside the scene)
		applyEditorWindowMode(
			mLevelEditor.showLevelEditor
			|| mAnimator.showAnimationEditor
			|| mScriptEditor.showScriptEditor);

		// the level editor's scene-zoom setting can change every frame;
		// re-apply the view whenever the effective (window-adapted) zoom
		// baked into it differs so the scene region grows/shrinks live
		if (mEditorLayoutActive
			&& mAppliedSceneZoom
			   != mLevelEditor.sceneZoomForWindow(
					static_cast<float>(mWindow.getSize().x),
					static_cast<float>(mWindow.getSize().y)))
		{
			applyLetterboxView(mWindow.getSize());
		}

		// ImGui frame: NewFrame, our widget calls and Render must happen exactly
		// once per rendered frame (not per simulation tick).
		ImGui::SFML::Update(mWindow, imguiClock.restart());
		if (!mIsPaused)
		{
			mDebugWidgets.CreateDebugGui();
		}//!mAnimator.showAnimationEditor
		mAnimator.createEditorWidgets(&mAnimator.showAnimationEditor);
		mLevelEditor.createEditorWidgets(&mLevelEditor.showLevelEditor);
		mScriptEditor.createEditorWidgets(&mScriptEditor.showScriptEditor);
		drawLogWindow();

		// while editors are  open the game only listens to the
		// mode keys (see PlayerController::handleEvent / handleRealtimeInput);
		// during the play-in-editor test the player controller gets its
		// input back so the level can be played inside the editor window
		mPlayerController->setUiInputActive(
			(mAnimator.showAnimationEditor
			 || mLevelEditor.showLevelEditor
			 || mScriptEditor.showScriptEditor)
			&& not mLevelEditor.testPlayingActive());

		// during the play-in-editor test the scene must look like the real
		// game: normal draw order and the normal character sprite. The b2
		// physics-debug overlay (P) stays available for on purpose
		const bool editorDebugVisuals = mLevelEditor.showLevelEditor
			&& not mLevelEditor.testPlayingActive();
		mWorld.setPlayerOnTop(editorDebugVisuals);
		mPlayerController->getPlayableCharacter()->setDebugFillMode(
			editorDebugVisuals);

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
		// arrow keys navigate the level-editor object list while it is
		// open; Ctrl+Z / Ctrl+Shift+Z / Ctrl+Y undo/redo the last edit
		if (mLevelEditor.showLevelEditor
			&& not mLevelEditor.testPlayingActive()
			&& event->is<sf::Event::KeyPressed>())
		{
			const auto* key = event->getIf<sf::Event::KeyPressed>();
			// Z/Y via the layout-localized key code (QWERTZ keyboards
			// report the 'Z' key as scancode Y)
			if (key->scancode == sf::Keyboard::Scancode::Up
				|| key->scancode == sf::Keyboard::Scancode::Down
				|| key->scancode == sf::Keyboard::Scancode::Left
				|| key->scancode == sf::Keyboard::Scancode::Right
				|| key->code == sf::Keyboard::Key::Z
				|| key->code == sf::Keyboard::Key::Y
				|| key->code == sf::Keyboard::Key::Tab)
			{
				mLevelEditor.handleKey(*key);
			}
		}

		// ESC ends the play-in-editor test while the editor stays open;
		// the keypress is swallowed so it never reaches the player.
		// The mode keys (F1-F12 toggles) are kept inert during the test
		// so panels can't be switched mid-test; P still toggles the b2
		// physics-debug overlay (hitboxes, raycasts, pivot points)
		bool swallowEvent = false;
		if (mLevelEditor.showLevelEditor
			&& mLevelEditor.testPlayingActive()
			&& event->is<sf::Event::KeyPressed>())
		{
			const auto* testKey = event->getIf<sf::Event::KeyPressed>();
			if (testKey->code == sf::Keyboard::Key::Escape)
			{
				mLevelEditor.setTestPlaying(false);
				swallowEvent = true;
			}
			else if (testKey->scancode >= sf::Keyboard::Scancode::F1
					 && testKey->scancode <= sf::Keyboard::Scancode::F12)
			{
				swallowEvent = true;
			}
		}
		// P toggles the b2 physics-debug overlay (hitboxes, raycasts,
		// pivot points) in EVERY mode: normal play, open editor and
		// play test. The game triggers the toggle directly and
		// swallows the key, because the player controller's UI-input
		// gate would swallow P while an editor is open otherwise.
		// Never fires while typing in an editor text field.
		if (not swallowEvent
			&& event->is<sf::Event::KeyPressed>()
			&& event->getIf<sf::Event::KeyPressed>()->scancode
				== sf::Keyboard::Scancode::P
			&& not ImGui::GetIO().WantTextInput)
		{
			mPlayerController->requestDebugToggle();
			swallowEvent = true;
		}
		if (not swallowEvent)
		{
			mPlayerController->handleEvent(event.value(), commands);
		}

		// forward game-view mouse events to the level editor while it is
		// open (not during the play-in-editor test: the clicks belong to
		// the running game then)
		if (mLevelEditor.showLevelEditor
			&& not mLevelEditor.testPlayingActive())
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
			// button press, button release); in the enlarged editor window only
			// the top-left canvas region is scene, clicks in the widget margins
			// must not paint/select into the level
			const bool inCanvasRegion = mousePixel.x >= 0
				&& mousePixel.y >= 0
				&& (not mEditorLayoutActive
					|| (mousePixel.x
							< static_cast<int>(CANVAS_RESOLUTION.x
												 * mAppliedSceneZoom)
						&& mousePixel.y
							< static_cast<int>(CANVAS_RESOLUTION.y
																	* mAppliedSceneZoom)));
			if (mousePixel != sf::Vector2i{-1, -1} && inCanvasRegion)
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
			// keep the window layout across sessions
			saveEditorLayout();
			mWindow.close();
		}

		if (const auto* resized = event->getIf<sf::Event::Resized>())
		{
			applyLetterboxView(resized->size);
		}
}

namespace
{
constexpr std::size_t kMaxLogLines = 500;
}

void
Game::setupLogCapture()
{
	if (pipe(mLogPipeFds.data()) != 0)
	{
		fmt::print("Game: could not create the log-capture pipe.\n");
		return;
	}
	// keep a duplicate of the real stdout so the console still gets the
	// log output; the window gets its copy through the pipe
	mConsoleFd = dup(STDOUT_FILENO);
	fflush(stdout);
	if (dup2(mLogPipeFds[1], STDOUT_FILENO) < 0)
	{
		fmt::print("Game: could not redirect stdout into the log pipe.\n");
		return;
	}
	// make stdout line-buffered so every fmt::print line arrives in the
	// log window promptly (a pipe would otherwise be fully buffered)
	setvbuf(stdout, nullptr, _IOLBF, 0);
	mLogReader = std::thread([this]()
	{
		std::string pending;
		char buffer[1024];
		while (true)
		{
			const ssize_t n = read(mLogPipeFds[0], buffer, sizeof(buffer));
			if (n <= 0) { break; }
			write(mConsoleFd, buffer, static_cast<size_t>(n));
			pending.append(buffer, static_cast<size_t>(n));
			std::lock_guard<std::mutex> lock(mLogMutex);
			std::size_t start = 0;
			while (true)
			{
				const auto nl = pending.find('\n', start);
				if (nl == std::string::npos) { break; }
				mLogLines.push_back(pending.substr(start, nl - start));
				if (mLogLines.size() > kMaxLogLines)
				{
					mLogLines.pop_front();
				}
				start = nl + 1;
			}
			pending.erase(0, start);
		}
	});
	// runs until process exit; the window reads the captured lines
	mLogReader.detach();
}

void
Game::drawLogWindow()
{
	if (not mEditorLayoutActive) { return; }
	const float sceneBottom = CANVAS_RESOLUTION.y * mAppliedSceneZoom + 12.f;
	ImGui::SetNextWindowPos(ImVec2(12.f, sceneBottom),
	                       ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(
		ImVec2(CANVAS_RESOLUTION.x * mAppliedSceneZoom, 240.f),
		ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Output Log", nullptr))
	{
		if (ImGui::Button("Clear"))
		{
			std::lock_guard<std::mutex> lock(mLogMutex);
			mLogLines.clear();
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%d lines", [this]()
		{
			std::lock_guard<std::mutex> lock(mLogMutex);
			return static_cast<int>(mLogLines.size());
		}());
		ImGui::Separator();
		if (ImGui::BeginChild("##logScroll", ImVec2(0.f, 0.f)))
		{
			// stick to the bottom while the user has not scrolled up
			const bool atBottom = ImGui::GetScrollY()
				>= ImGui::GetScrollMaxY() - 8.f;
			std::lock_guard<std::mutex> lock(mLogMutex);
			if (mLogLines.empty())
			{
				ImGui::TextDisabled("(no output yet)");
			}
			for (const auto& line : mLogLines)
			{
				ImGui::TextUnformatted(line.c_str());
			}
			if (atBottom && not mLogLines.empty())
			{
				ImGui::SetScrollHereY(1.f);
			}
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

void
Game::setLevelEditorOpen(bool open)
{
	mLevelEditor.showLevelEditor = open;
	// keep the character visible while editing: scene objects are
	// attached above the player, so the player moves to the top layer
	// and is drawn as a debug single-color fill
	mWorld.setPlayerOnTop(open);
	mPlayerController->getPlayableCharacter()->setDebugFillMode(open);
}

void
Game::saveEditorLayout()
{
	// in editor mode the play window size/position are the remembered ones
	const sf::Vector2u size = mEditorLayoutActive ? mPlayWindowSize
													 : mWindow.getSize();
	const sf::Vector2i pos = mEditorLayoutActive ? mPlayWindowPos
														 : mWindow.getPosition();
	const nlohmann::json layout = {
		{"window",
		 {{ "w", size.x },
		  { "h", size.y },
		  { "x", pos.x },
		  { "y", pos.y }}},
		{"sceneZoom", mLevelEditor.sceneZoom()},
		{"editors",
		 {{ "level", mLevelEditor.showLevelEditor },
		  { "animator", mAnimator.showAnimationEditor },
		  { "script", mScriptEditor.showScriptEditor }}},
	};
	std::ofstream oLayout(mEditorLayoutPath);
	if (not oLayout.is_open())
	{
		fmt::print("Game: could not open {} for writing.\n",
				   mEditorLayoutPath);
		return;
	}
	oLayout << layout.dump(2) << "\n";
	// the ImGui window positions live in the ImGui layout file; flush
	// them now so the editor arrangement is immediately persistent
	// (ImGui's own periodic saving is disabled, see run())
	ImGui::SaveIniSettingsToDisk(mImGuiLayoutPath.c_str());
	fmt::print("Game: saved editor layout to {} (zoom {:.2f}x, play window {}x{})\n",
			   mEditorLayoutPath, mLevelEditor.sceneZoom(), size.x, size.y);
}

void
Game::loadEditorLayout()
{
	std::ifstream iLayout(mEditorLayoutPath);
	if (not iLayout.is_open()) { return; }
	try
	{
		const nlohmann::json layout = nlohmann::json::parse(iLayout);
		if (layout.contains("window"))
		{
			const auto& win = layout["window"];
			// sane clamps: accept the saved size only if it stays usable
			mPlayWindowSize = sf::Vector2u{
				std::clamp(win["w"].get<unsigned>(), 320u, 3840u),
				std::clamp(win["h"].get<unsigned>(), 180u, 2160u)};
			mWindow.setSize(mPlayWindowSize);
			mPlayWindowPos = sf::Vector2i{ win["x"].get<int>(),
										   win["y"].get<int>() };
			mWindow.setPosition(mPlayWindowPos);
			applyLetterboxView(mWindow.getSize());
		}
		if (layout.contains("sceneZoom"))
		{
			mLevelEditor.setSceneZoom(
				std::clamp(layout["sceneZoom"].get<float>(), 1.f, 6.f));
			fmt::print("Game: restored scene zoom {:.2f}x\n",
					   mLevelEditor.sceneZoom());
		}
		else
		{
			fmt::print("Game: editor layout has no sceneZoom entry "
					"(staying at default {:.2f}x)\n",
					mLevelEditor.sceneZoom());
		}
		if (layout.contains("editors"))
		{
			// restore exactly the editor combination that was open when the
			// layout was saved (the run loop opens the fullscreen editor
			// window automatically as soon as any editor is open)
			const auto& editors = layout["editors"];
			if (editors.value("level", false)) { setLevelEditorOpen(true); }
			mAnimator.showAnimationEditor = editors.value("animator", false);
			mScriptEditor.showScriptEditor = editors.value("script", false);
		}
		fmt::print("Game: loaded editor layout from {}\n", mEditorLayoutPath);
	}
	catch (const std::exception& e)
	{
		fmt::print("Game: could not read editor layout {}: {}\n",
				   mEditorLayoutPath, e.what());
	}
}

void
Game::applyEditorWindowMode(bool editorOpen)
{
	if (editorOpen == mEditorLayoutActive) { return; }
	mEditorLayoutActive = editorOpen;
	if (mEditorLayoutActive)
	{
		// editor mode: a borderless, desktop-sized window (no title bar
		// overflow, no taskbar gap) gives the widgets room next to the
		// scene; remember the playing window's size AND position so
		// closing the editors returns to exactly the setup the player
		// had (a manually resized play window survives the editor session)
		mPlayWindowSize = mWindow.getSize();
		mPlayWindowPos = mWindow.getPosition();
		// SFML 3: styles can only be set at creation, so the window is
		// recreated (World/Renderer hold references, which stay valid);
		// recreation emits no Resized event, the view is applied below
		mWindow.create(
			sf::VideoMode(sf::VideoMode::getDesktopMode().size),
			PROJECT_NAME.data(), sf::Style::None);
		mWindow.setPosition({0, 0});
	}
	else
	{
		mWindow.create(sf::VideoMode(mPlayWindowSize), PROJECT_NAME.data(),
			sf::Style::Default);
		mWindow.setPosition(mPlayWindowPos);
		// back in play mode: the layout (window sizes, open editors) is
		// worth persisting now that the editor session is complete
		saveEditorLayout();
	}
	applyLetterboxView(mWindow.getSize());
}


void
Game::applyLetterboxView(sf::Vector2u newSize)
{
	if (newSize.x == 0 || newSize.y == 0) { return; }

	sf::View view(sf::FloatRect(sf::Vector2f{}, CANVAS_RESOLUTION));
	if (mEditorLayoutActive)
	{
		// editor mode: the scene covers the canvas resolution and is scaled
		// up by the editor's scene-zoom factor, pinned to the top-left
		// corner of the enlarged window; everything right/below stays free
		// for the editor widgets. the chosen (persisted) zoom is only
		// adapted to the window here, never modified
		mAppliedSceneZoom = mLevelEditor.sceneZoomForWindow(
			static_cast<float>(newSize.x), static_cast<float>(newSize.y));
		view.setViewport(sf::FloatRect(
			{ 0.f, 0.f },
			{ CANVAS_RESOLUTION.x * mAppliedSceneZoom
			    / static_cast<float>(newSize.x),
			  CANVAS_RESOLUTION.y * mAppliedSceneZoom
			    / static_cast<float>(newSize.y) }));
		mWindow.setView(view);
		return;
	}
	mAppliedSceneZoom = 1.f;
	// keep the 16:9 canvas aspect: the view covers the canvas resolution
	// and its viewport is letterboxed (centered black bars on the
	// overflowing side); ImGui is unaffected (own projection) and
	// mapPixelToCoords stays correct through the view's viewport
	const float canvasAspect = CANVAS_RESOLUTION.x / CANVAS_RESOLUTION.y;
	const float windowAspect
		= static_cast<float>(newSize.x) / static_cast<float>(newSize.y);

	if (windowAspect > canvasAspect)
	{
		// window too wide: bars left and right
		const float width = canvasAspect / windowAspect;
		view.setViewport(sf::FloatRect({ (1.f - width) / 2.f, 0.f },
									  { width, 1.f }));
	}
	else
	{
		// window too high: bars top and bottom
		const float height = windowAspect / canvasAspect;
		view.setViewport(sf::FloatRect({ 0.f, (1.f - height) / 2.f },
									  { 1.f, height }));
	}
	mWindow.setView(view);
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

	// selection mode pauses the physics world stepping so dragged bodies
	// don't fall between the drag updates; leaving it resumes automatically
	// (and during the play-in-editor test the simulation always runs)
	mWorld.setPhysicsActive(
		mLevelEditor.testPlayingActive()
		|| not mLevelEditor.transformModeActive());
	mWorld.update(deltaTime);

	// the animator preview must be advanced with the simulation; the
	// ImGui-filebrowser is handled inside the ImGui-frame (createEditorWidgets)
	mAnimator.update(deltaTime);
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
