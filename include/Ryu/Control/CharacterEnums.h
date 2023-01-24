#pragma once

#include <Thirdparty/betterEnum/enum.h>

enum class EInput 
{
    None,
    PressUp,
    PressDown,
    PressRight,
    PressLeft,
    ReleaseUp,
    ReleaseDown,
    ReleaseRight,
    ReleaseLeft,
};

BETTER_ENUM(ECharacterState, char,None=0,Idle,Walk,Run,Falling);

/*
enum class ECharacterState{
    None,
    Idle,
    Walk,
    Run,
    Falling,
    //...
};*/

enum class EMoveDirection
{
    None=0,
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight
};

enum class EClimbHeight{
    None=0,
    Low,
    Mid,
    High,  
};

enum class ECharacterMovement{
    // A character is capable do several things at a time. to reduce complexity we combine movements with OR
    // to check if we can do a certain movement we then use AND
    None                = 0,
    CanClimb            = 1 << 0,    //0000 0001
    CanLadder           = 1 << 1,    //0000 0010
    CanPush             = 1 << 2,    //0000 0100
    CanPull             = 1 << 3,    //0000 1000
      
};