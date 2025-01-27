set (SRC_TEMPLATE 
      "TemplateTest.cpp"
    )

find_package(GTest CONFIG REQUIRED)
find_package(SFML 3.0.0 COMPONENTS Audio Graphics Window System Network REQUIRED)
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
  Template_test PUBLIC
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
  SFML::Graphics
  SFML::Window
  SFML::System
  SFML::Audio
  SFML::Network
	nlohmann_json::nlohmann_json 
)

include(GoogleTest)
gtest_discover_tests(Template_test)

#add_library(ryudebuglib SHARED "${SRC}" ) #"${HEADER}")

target_include_directories(Template_test PUBLIC "${RyuPrototype_SOURCE_DIR}/include")

add_subdirectory("Animation")
