#pragma once

#include <Ryu/Control/CharacterEnums.h>
#include <SFML/System/Vector2.hpp>

// this is the framesize for the boundary box of the physics body
// (old sheet: 80x96 frames -> 60x86 box; 64x64 sheet keeps proportionally
// more slack around the ~28x53 sprite content, duck box covers the content
// below the frame center, ~29px + slack)
static constexpr std::pair<int, int> INIT_FRAME_SIZE(30, 55);

// Ledge-hang/climbup sprite anchoring: in the 80x128 ledge frames of the
// combined Ichi64x64 atlas (ichi_level1.png) the ledge corner (wall face
// meeting the ledge top) sits at in-frame pixel (35, 60) while the frames
// are anchored at their center (40, 64) via setAnimationPosition. To place
// the hands on the corner, the frame center must sit
//   x: corner + dir * (40 - 35)  (mirrored when facing left)
//   y: corner + (64 - 60)
// relative to the raycast corner (which points dir*2 INTO the wall).
// Measured from the hang frame (944,0) and climbup frame sequence
// (fixed camera: frame #0 hands at y~54, frame #14 standing feet at y~61
// share the ledge-top line y~60); verified by compositing the hang frame
// over the level2 right_side wall (hands land on the corner).
constexpr float LEDGE_FRAME_GRIP_X = 35.f;
constexpr float LEDGE_FRAME_GRIP_Y = 60.f;
constexpr float LEDGE_FRAME_ANCHOR_X = 40.f;
constexpr float LEDGE_FRAME_ANCHOR_Y = 64.f;
static constexpr std::pair<int, int> DUCK_FRAME_SIZE(30, 34);

/// Ledge-hang collider (see CharacterStateLedgeHang::enter): narrower and
/// taller than the stand box, and pushed a gap away from the wall face so
/// it cannot overlap/penetrate the ledge colliders while hanging (an
/// overlapped body gets depenetrated upward/downward by Box2D and the
/// character used to get stuck instead of dropping on PressDown).
static constexpr std::pair<int, int> HANG_FRAME_SIZE(24, 68);
/// gap (pixels) between the hang collider's front edge and the wall face
/// (the raycast corner points dir*2 INTO the wall, the wall face sits 2px
/// back towards the character)
static constexpr float LEDGE_HANG_WALL_GAP_PX = 2.f;

/// Corner rounding of the character collider (pixels). The collider is a
/// ROUNDED BOX whose TOTAL extents stay INIT_FRAME_SIZE/DUCK_FRAME_SIZE:
/// Box2D EXPANDS shapes beyond their vertices by the polygon radius, so
/// the inner box is inset by this amount (see Physics::initCharacterPhysics).
/// Bigger = rounder (slides better over tile seams), smaller = boxier.
/// 8px rounds the bottom corners enough to slide across adjacent tiles.
static constexpr float CHARACTER_COLLIDER_ROUNDING_PX = 8.f;

class ICharacter {
public:
    virtual sf::Vector2f getPosition() = 0;
    virtual void setBodyId(b2BodyId bodyId) = 0;
    virtual ECharacters getCharacterName() = 0;
    virtual ~ICharacter() = default;

};
