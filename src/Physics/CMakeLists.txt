set (SRC 
     "DebugDraw.cpp"
     "Physics.cpp"
     "RaycastComponent.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/DebugDraw.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/Physics.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/PhysicsObject.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/Raycast.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/RaycastComponent.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Physics/RaycastTypes.h"
)

add_library(ryuphysicslib STATIC "${SRC}" "${HEADER}")

target_include_directories(ryuphysicslib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryuphysicslib)
