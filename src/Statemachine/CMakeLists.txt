set (SRC 
     "CharacterStateClimb.cpp"
     "CharacterStateIdle.cpp"
     "CharacterStateDuckEnter.cpp"
     "CharacterStateDuckIdle.cpp"
     "CharacterStateDuckWalk.cpp"
     "CharacterStateRun.cpp"
     "CharacterStateJumpUp.cpp"
     "CharacterStateJumpNormal.cpp"
     "CharacterStateFalling.cpp"
     "CharacterStateFallingEnd.cpp"
     "CharacterStateLedgeHang.cpp"
     "CharacterStateLedgeClimbUp.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateClimb.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateIdle.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateDuckEnter.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateDuckIdle.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateDuckWalk.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateRun.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateJumpUp.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateJumpNormal.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateFalling.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateFallingEnd.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateLedgeHang.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Statemachine/CharacterStateLedgeClimbUp.h"
)

add_library(ryustateslib STATIC "${SRC}" "${HEADER}")

target_link_libraries(ryustateslib PUBLIC ryucorelib)

target_include_directories(ryustateslib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryustateslib)
