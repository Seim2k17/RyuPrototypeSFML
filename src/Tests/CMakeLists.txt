set (SRC_TEMPLATE 
      "TemplateTest.cpp"
    )

find_package(GTest CONFIG REQUIRED)
#[[]  
set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Debug/b2DrawSFML.hpp"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Debug/imGuiDebug.h"
)
]]
enable_testing()

add_executable(
  Template_test
	"${SRC_TEMPLATE}"
)

target_link_libraries(
  Template_test
  GTest::gtest_main
	# ryucorelib 
 #  ryudebuglib
	# ryuscenelib	
 #  ryueventslib
	# ryucontrollib
	# ryuphysicslib
	# ryusuitelib
	ryuanimationlib
	# ryucharacterlib
	# ryustateslib
  fmt::fmt
	ImGui-SFML::ImGui-SFML
	nlohmann_json::nlohmann_json 
)

include(GoogleTest)
gtest_discover_tests(Template_test)

#add_library(ryudebuglib SHARED "${SRC}" ) #"${HEADER}")

target_include_directories(Template_test PUBLIC "${RyuPrototype_SOURCE_DIR}/include")
#link_sfml_to_target(Template_test)

add_subdirectory("Animation")
