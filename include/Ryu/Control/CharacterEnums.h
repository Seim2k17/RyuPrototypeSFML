#pragma once

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

enum class ECharacterState{
    None,
    Idle,
    Walk,
    Run,
    //...
};

enum class EMoveDirecton
{
    None,
    Up,
    Down,
    Left,
    Right,
    UpLeft,
    UpRight,
    DownLeft,
    DownRight
};