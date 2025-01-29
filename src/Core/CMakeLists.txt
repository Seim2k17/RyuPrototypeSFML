set (SRC
    "Command.cpp"
    "CommandQueue.cpp"
    "Game.cpp"
    "SpriteNode.cpp"
    "Utilities.cpp"
    "World.cpp"
)

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/AssetIdentifiers.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/AssetManager.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/AssetManager.inl"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/Category.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/Command.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/CommandQueue.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/Game.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/SpriteNode.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/Utilities.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Core/World.h"
)

add_library(ryucorelib STATIC "${SRC}" "${HEADER}")

target_link_libraries(ryucorelib PUBLIC
  ryuanimationlib
  ryucontrollib
  ryusuitelib
  ryudebuglib
  box2d
  ImGui-SFML::ImGui-SFML
  pthread # needed by tracy
  dl
  TracyClient
)

target_include_directories(ryucorelib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryucorelib)
