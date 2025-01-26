# could take a lok and test: https://github.com/friendlyanon/cmake-init

cmake_minimum_required(VERSION 3.28)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

project(RyuPrototype VERSION 1.0
        DESCRIPTION "SFML prototype of RYU - a combat based cinematic puzzle platformer"
        LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 20)

# fetch SFML from git
include(FetchContent)
message(STATUS "Fetching SFML...")
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_MakeAvailable(SFML)
message(STATUS "SFML fetched and built.")

# Create a function to simplify linking libraries
function(link_sfml_to_target target)
    target_link_libraries(${target} PUBLIC
        SFML::Graphics
        SFML::Window
        SFML::System
        SFML::Audio
        SFML::Network
    )
endfunction()

set(MAIN "src/Main.cpp")
set(INCLUDE_FOLDER "src/include")
#set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake/;${CMAKE_MODULE_PATH}")

#set(CMAKE_CXX_COMPILER "gcc")

#find_package(SFML 3.0.0 COMPONENTS Audio Graphics Window System Network REQUIRED)
find_package(box2d CONFIG REQUIRED)
find_package(nlohmann_json 3.2.0 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ImGui-SFML REQUIRED)
find_package(fmt)
	
if(ImGui-SFML_FOUND)
	message("\n-> ImGui-SFML directory found.")
else()
	message("\n-> ImGui-SFML directory not found.")
endif()

# If found, include and link; otherwise output error message
if(SFML_FOUND)
	message("-- Include SFML: $ENV{SFML_INCLUDE_DIR}")
	#include_directories(${SFML_INCLUDE_DIR})
else()
	#set(SFML_ROOT "" CACHE PATH "SFML top-level directory")
	message("\n-> SFML directory not found. Set SFML_ROOT to SFML's top-level path (containing \"include\" and \"lib\" directories).")
	message("-> Make sure the SFML libraries with the same configuration (Release/Debug, Static/Dynamic) exist.\n")
endif()

#set(SFML_LIBRARIES "SFML::Audio SFML::Graphics SFML::Window SFML::System SFML::Network")
#set(SFML_DEPENDENCIES "libxi-dev")

# first add global find packages !!! -> then add sub CMakeLists -> the sub CMakes have all the find packages
add_subdirectory("src") 
add_executable(ryupt "${MAIN}")
target_compile_features(ryupt PRIVATE cxx_std_20)

target_include_directories(ryupt PUBLIC "${INCLUDE_FOLDER}") #HOW >TO ADD SUBFOLDErS ?
#message("-- Linking SFML: found in ${SFML_DIR}, Libraries: ${SFML_LIBRARIES}")
link_sfml_to_target(ryupt)
target_link_libraries(ryupt PUBLIC
	#SFML::Graphics SFML::Audio SFML::Window SFML::System SFML::Network
	#sfml-audio sfml-graphics sfml-window sfml-system sfml-network
	ryucorelib
	ryudebuglib
	ryuscenelib	
	ryueventslib
	ryucontrollib
	ryuphysicslib
	ryusuitelib
	ryuanimationlib
	ryucharacterlib
	ryustateslib
	box2d	
	fmt::fmt
	ImGui-SFML::ImGui-SFML
	#${SFML_LIBRARIES}
	#${SFML_DEPENDENCIES}
	nlohmann_json::nlohmann_json
)# b2DebugDraw)

#add_subdirectory("src/Tests")
