set (SRC 
      "PlayerController.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Control/CharacterEnums.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Control/PlayerController.h"
)

add_library(ryucontrollib STATIC "${SRC}" "${HEADER}")

target_link_libraries(ryucontrollib PUBLIC
  ryucorelib
  pthread # needed by tracy
  dl
  TracyClient
)

target_include_directories(ryucontrollib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryucontrollib)
