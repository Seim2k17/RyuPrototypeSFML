set (SRC 
      "Box.cpp"
      "Crate.cpp"
      "Entity.cpp"
      "EntityStatic.cpp"
      "LevelManager.cpp"
      "SceneNode.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/Box.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/ContactListener.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/Crate.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/Entity.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/EntityStatic.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/LevelManager.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Scene/SceneNode.h"
)

add_library(ryuscenelib SHARED "${SRC}" "${HEADER}")

target_include_directories(ryuscenelib PUBLIC 
    "${RyuPrototype_SOURCE_DIR}/include"
)

link_sfml_to_target(ryuscenelib)
