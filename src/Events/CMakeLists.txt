set (SRC 
      "Subject.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Events/EventEnums.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Events/Observer.h"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Events/Subject.h"
)

add_library(ryueventslib SHARED "${SRC}" "${HEADER}")

target_include_directories(ryueventslib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryueventslib)
