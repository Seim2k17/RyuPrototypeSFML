#pragma once

#include "Ryu/Core/EventManager.h"
#include <SFML/Graphics.hpp>
#include <Ryu/Character/CharacterIchi.h>
#include <Ryu/Control/PlayerController.h>
#include <Ryu/Core/World.h>
#include <Ryu/Debug/imGuiDebug.h>
#include <Ryu/Events/Observer.h>
#include <RyuSuite/RAnimator.h>
#include <RyuSuite/LevelEditor.h>
#include <RyuSuite/ScriptEditor.h>

#include <memory>
#include <optional>

class CharacterIchi;
class EventManager;
class PlayerController;
class CommandQueue;

//namespace ryu{

class Game : public Observer
{
  public:
	  Game();
	  void run();
    World& getWorld() {return mWorld;}
    void onNotify(const SceneNode& entity, RyuEvent event) override;
  private:
	  void processEvents(std::optional<sf::Event> event, CommandQueue& commands);
	  /// letterboxes the window view on resize so the 16:9 canvas aspect
	  /// ratio is kept (black bars on the overflowing side)
	  void applyLetterboxView(sf::Vector2u newSize);
	  /// switches the game window between the small canvas-sized playing
	  /// window and a large editor window whenever an editor is open so the
	  /// editor widgets have room next to the scene instead of covering it
	  void applyEditorWindowMode(bool editorOpen);
	  void update(sf::Time deltaTime);
	  void render();
    // TODO: clarify where exactly to put these methods
	  void handleUserInput(sf::Keyboard::Key key, bool keyPressed);
	  void addObservers();
    void setDebugValues();
    void useCharacterDebugSettings();
    void teleportMainCharacter(float x, float y);

  private:
        sf::RenderWindow mWindow;
        EventManager mEventManager;
        std::unique_ptr<PlayerController> mPlayerController;
        RyuAnimator::Editor mAnimator;
        RyuLevel::Editor mLevelEditor;
        RyuScript::Editor mScriptEditor;
        bool mIsPaused;
        RyuDebug::DebugWidgets mDebugWidgets;
        World mWorld;
        bool mDebugWidgetsActive;
        bool mEditorLayoutActive{false};
        /// scene-zoom factor last baked into the window view (the view is
        /// re-applied when the level editor's zoom setting changes)
        float mAppliedSceneZoom{0.f};
        /// size/position of the playing window when the editor mode was
        /// entered; restored on leave so a manually resized play window
        /// survives the editor session
        sf::Vector2u mPlayWindowSize{640u, 360u};
        sf::Vector2i mPlayWindowPos{32, 32};
	//ryu::AssetManager<sf::Font,std::string> fontManager;
};

//} /// namespace ryu
