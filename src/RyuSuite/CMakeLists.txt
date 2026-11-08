set (SRC 
     "RAnimator.cpp"
     "LevelEditor.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/RyuSuite/RAnimator.h"
     "${RyuPrototype_SOURCE_DIR}/include/RyuSuite/LevelEditor.h"
)

add_library(ryusuitelib STATIC "${SRC}" "${HEADER}")

target_link_libraries(ryusuitelib PUBLIC
  ImGui-SFML::ImGui-SFML
  pthread # needed by tracy
  dl
  TracyClient
)

target_include_directories(ryusuitelib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
target_include_directories(ryusuitelib PUBLIC "${box2d_SOURCE_DIR}/include")
target_include_directories(ryusuitelib PUBLIC "${nlohmann_json_SOURCE_DIR}/include") 
target_include_directories(ryusuitelib PUBLIC "${fmt_SOURCE_DIR}/include") 
link_sfml_to_target(ryusuitelib)
