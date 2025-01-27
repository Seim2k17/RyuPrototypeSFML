set (SRC_TEMPLATE 
      "JsonParserTest.cpp"
    )

find_package(SFML 3.0.0 COMPONENTS Audio Graphics Window System Network REQUIRED)
find_package(GTest CONFIG REQUIRED)
#[[]  
set(HEADER
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Debug/b2DrawSFML.hpp"
     "${RyuPrototype_SOURCE_DIR}/include/Ryu/Debug/imGuiDebug.h"
)
]]
enable_testing()

add_executable(
  JsonParser_test
	"${SRC_TEMPLATE}"
)

target_link_libraries(
  JsonParser_test PUBLIC
  GTest::gtest_main
	ryuanimationlib
  fmt::fmt
	#${SFML_LIBRARIES}
	#${SFML_DEPENDENCIES}
	ImGui-SFML::ImGui-SFML
  SFML::Graphics
  SFML::Window
  SFML::System
  SFML::Audio
  SFML::Network
	nlohmann_json::nlohmann_json
)

include(GoogleTest)
gtest_discover_tests(JsonParser_test)

target_include_directories(JsonParser_test PUBLIC "${RyuPrototype_SOURCE_DIR}/include")

