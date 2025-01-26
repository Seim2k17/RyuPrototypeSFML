set (SRC 
     "DebugDraw.cpp"
     "RaycastComponent.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/DebugDraw.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/PhysicsObject.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/Raycast.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/RaycastComponent.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/RaycastTypes.h"
)

add_library(ryuphysicslib SHARED "${SRC}" "${HEADER}")

target_include_directories(ryuphysicslib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryuphysicslib)
