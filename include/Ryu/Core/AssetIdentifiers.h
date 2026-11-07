#pragma once
#include <Thirdparty/betterEnum/enum.h>
#include <variant>

// Forward declaration of SFML classes
namespace sf
{
	class Texture;
}

//namespace ryu{
// TODO: place it in AnimationData.h !!! and use it for editor AND game !!!!

namespace Textures
{
	enum class SceneID
	{
		BGMountain,
		BoxPushable,
		BoxHeavy,
		Button,
		Grass,
		Grate,
		Ground,
		Ladder,
		Teleport,
		Unknown
	};

	enum class PhysicAssetsID
	{
		Empty,
		Crate,
};

    BETTER_ENUM(AnimationType, char,
                None=0,
                Character,
                Scene
                );

	BETTER_ENUM(LevelID, char,
		Unknown=0,
		Level1,
		Level2
	);

	BETTER_ENUM(SpritesheetID, char,
		Unknown=0,
		Ichi64x64,
        Ichi128x196
    );

	// TODO: sub-spritesheetIDs in character itself ? (e.g. RUN in the states)
  // Rename ? to CharacterAnimationId
    BETTER_ENUM(CharacterID, char ,
        None=0,
		IchiIdle, //
        IchiRun, //
		IchiStartFalling, //
		IchiFallingLoop, //
        IchiLandEnter, //                 //
		IchiLandLow, //
		IchiLandHigh, //
		IchiDuckEnter, //
		IchiDuckIdle, //
		IchiDuckWalk, //
        IchiLadderEnter, //
        IchiLadderBack, //
        IchiLadderIdle,
		IchiLadderClimbLoop, //
		IchiJumpUp, //
		IchiJumpForwardNrml, //
		IchiSword1Idle, //
		IchiSword1Walk, //
		IchiWalk, //
		IchiLedgeClimbUp, //
		IchiLedgeHangIdle, //
        Length = 22 //
		// ... Spritesheets
    );

    BETTER_ENUM(SceneBGAni, char,
                // TODO: fill for SceneBackgroundAnimationIds
                None=0
    );

	enum class GuiID {
			 None,
			 ForwardFrame,
			 BackwardFrame,
			 Play,
			 Stop,
			 EndFrame,
			 StartFrame,
			 Frame,
			 FrameBorder,	
    };


}


// Forward declaration and a few type definitions
template <typename Resource, typename Identifier>
class AssetManager;

typedef AssetManager<sf::Texture, Textures::SceneID> SceneTextureHolder;

//}
