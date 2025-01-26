set (SRC
     "AnimationManager.cpp"
     "SpritesheetAnimation.cpp"
     "JsonParser.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Animation/AnimationManager.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Animation/AnimationData.h"
     "${RyuPrototype_SOURCE_DIR}/include/RyuSuite/RAnimator.h"
     "${RyuPrototype_SOURCE_DIR}/include/Thirdparty/imgui-filebrowser/imfilebrowser.h"
)

add_library(ryuanimationlib STATIC "${SRC}" "${HEADER}" )

target_link_libraries(ryuanimationlib PUBLIC ryueventslib ryucharacterlib)

target_include_directories(ryuanimationlib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryuanimationlib)
