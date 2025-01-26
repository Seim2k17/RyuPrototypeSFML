set (SRC 
     "RAnimator.cpp"
    )

set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/RyuSuite/RAnimator.h"
)

add_library(ryusuitelib STATIC "${SRC}" "${HEADER}")

target_link_libraries(ryusuitelib PUBLIC
	ImGui-SFML::ImGui-SFML)

target_include_directories(ryusuitelib PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
link_sfml_to_target(ryusuitelib)
