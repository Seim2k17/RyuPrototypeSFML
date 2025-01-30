# RyuPrototypeSFML

Ryu prototype build finally with SFML .

But before starting with Ryu one need to learn SFML. Here we did with the SFML Game Development book.
See [License.txt]. in 02_SFML_GD_BOOK.

# Prerequisites

1. SFML v 3.0.0
    - see https://en.sfml-dev.org/ git source: https://github.com/SFML/SFML
    - https://www.sfml-dev.org/download/ download v3 for linux
    - tar xf SFML-3.0.0-linux-gcc-64-bit.tar.gz, remember path to extracted SFML-3.0.0 content
    - (sudo apt install libsfml-dev)
2. Box2D for physics v 2.4.1
    - clone it from: git@github.com:erincatto/box2d.git
     (./build.sh for static library)
    - mkdir build, cd build && cmake -DBUILD_SHARED_LIBS=on ..
        - if its not building deactivate the build for testbed in CMakeLists.txt     
    - sudo make install
    (alternatively  if Box2D is available on your system you can: sudo apt install libbox2d-dev)
3. Dear ImGui >=v1.8
    - see https://github.com/ocornut/imgui
    - clone git@github.com:ocornut/imgui.git    
4. ImGui-SFML >=v3.0
 a) - install OpenGL dev header: "sudo apt install libgl1-mesa-dev"
 b)  - see https://github.com/eliasdaler/imgui-sfml
     - clone from: git@github.com:eliasdaler/imgui-sfml.git
     - in ~/.bashrc add: 
        - export SFML_DIR=<path-to-local-SFML-3.0.0-repo>
        - export SFML_INCLUDE_DIR=$SFML_DIR:/include/:
        
     - cmake <path-to-local-ImGui-SFML-repo>  -DBUILD_SHARED_LIBS=ON -DIMGUI_SFML_IMGUI_DEMO=ON -DIMGUI_DIR=<path-to-local-imgui-repo>
     - (sudo) cmake --build <path-to-ImGui-SFML-repo-build-folder>  --target install
5. nlohmann/json
    - see & clone git@github.com:nlohmann/json.git
    - mkdir build && cd build
    - cmake.., make, make install
6. BETTER_ENUM library v.0.11.3
    - git@github.com:aantron/better-enums.git
7. googletest
    - (https://github.com/google/googletest/blob/main/googletest/README.md)
    - mkdir <somewhere-in-home>/googletest + cd into <somewhere>
    - git clone git@github.com:google/googletest.git + cd into googletest
    - mkdir build + cd build
    - cmake ..
    - make
    - sudo make install # install to /usr/local/ by default
8. fmt
    - git clone git@github.com:fmtlib/fmt.git
    - mkdir build, cd build && cmake -DBUILD_SHARED_LIBS=on ..
    - sudo make install
9. (imgui-filebrowser: https://github.com/AirGuanZ/imgui-filebrowser.git) IN EVALUATION

10. with 7041bf6aaefa62e17ecc7894f8ac792ca182a940 SMFL 3 & ImGui-SFML & Tracy is directly downloaded and builded. If there are configuring issues please check the following:
      - in /_deps/imguisfml-src/CMakeLists.txt: set(IMGUI_DIR "[folder-to-local-imgui-rep]")
      - if you want to use the profiler and building the server does not work 
      (in build/_deps/tracy-src/ do: 
         - cmake -B profiler/build -S profiler -DCMAKE_BUILD_TYPE=Release
         - cmake --build profiler/build --configure Release)
         -> set option(LEGACY ".." ON) & option(NO_PARALLEL_STL ".." ON) in  
         profiler/CMakeLists.txt
         - for meaningful profiling data use a Release build and one need to link tracy against  
         all the libraries in which classes you want to profile data
# after all prerequisities are installed to build Ryu
- in Ryu project folder:
    - (mkdir build && cd build)
    - cmake ..
    (- cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON)
    - make
    
