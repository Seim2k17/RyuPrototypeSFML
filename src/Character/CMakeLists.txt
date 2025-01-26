set (SRC 
     "CharacterBase.cpp"
     "CharacterIchi.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Character/CharacterBase.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Character/CharacterIchi.h"
)


add_library(ryucharacterlib STATIC "${SRC}" "${HEADER}")

target_link_libraries(ryucharacterlib PUBLIC ryuscenelib ryustateslib ryudebuglib ryucorelib ryuphysicslib box2d  )


target_include_directories(ryucharacterlib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryucharacterlib)
