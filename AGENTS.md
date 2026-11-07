# AGENTS.md — RyuPrototype project guide

## What this project is

RyuPrototype is a C++20 prototype of **RYU**, a combat-based cinematic puzzle
platformer. It is built on:

- **SFML 3.x** (fetched via CMake `FetchContent`) — windowing, rendering, sprites
- **Box2D v3.1** (fetched) — physics; note the y-axis points *down* (SFML
  coordinates), gravity is `(0, +9.81)`, and `Converter` in
  `include/Ryu/Core/Utilities.h` scales at `PIXELS_PER_METERS = 32`
- **ImGui + imgui-sfml** (fetched) — debug UI and the animation editor
- **Tracy profiler** (fetched, **disabled by default**, see below)
- **nlohmann/json**, **fmt**

Architecture highlights:

- `src/Core/Game.cpp` — main loop: fixed timestep `TimePerFrame = 17ms`
  (~60Hz), render once per frame *outside* the tick loop, sleeps when idle.
  ImGui NewFrame/widgets/Render happen exactly once per rendered frame.
- `src/Core/World.cpp` — owns `Physics` and `Renderer`.
- `src/Physics/Physics.cpp` — owns the Box2D world and **all** physics bodies
  (scene objects and characters); destroys them in `Physics::~Physics`. No
  other component may call `b2DestroyBody` (double-destroy hits
  `B2_ASSERT(b2Body_IsValid)` → SIGILL).
- `src/Core/Renderer.cpp` — scene graph; subscribes to physics events via the
  static `EventBus` (`include/Ryu/Events/EventBus.h`).
- The playable character (`CharacterIchi`) is owned by the `PlayerController`,
  which is a member of `Game` declared *before* `World` — so the character is
  destroyed *after* `Physics`. Keep this in mind for any destruction-order work.
- Assets are loaded with **relative paths** (`assets/...`), so the executable
  must be started from a directory that contains `assets/` (repo root or
  `build/`).

### Known pitfalls (as of writing)

- `Renderer::loadTextures()` references
  `assets/scenes/99_dummy/tile_grate_1.png`, which does not exist in the repo
  (only `grate.png`). Workaround locally: copy `grate.png` to
  `tile_grate_1.png`, otherwise the game aborts with "Exception thrown:
  AssetManager:load - Failed to load ...".
- `include/Thirdparty/imgui-filebrowser/imfilebrowser.h` and
  `include/Thirdparty/glm/glm.hpp` are referenced by the code but **not
  tracked in git**. If missing, fetch them:
  - imgui-filebrowser: single header from https://github.com/AirGuanZ/imgui-filebrowser
    → `include/Thirdparty/imgui-filebrowser/imfilebrowser.h`
  - glm: https://github.com/g-truc/glm → copy `glm/glm/*` to
    `include/Thirdparty/glm/` (needs `glm.hpp` at
    `include/Thirdparty/glm/glm.hpp`).
- **Tracy**: `TRACY_ENABLE` is set `OFF` before `FetchContent_MakeAvailable(TRACY)`
  in `CMakeLists.txt`. Do not revert this: with Tracy enabled and no Tracy
  server connected, the client leaks hundreds of MB/s (rpmalloc slabs) until
  OOM. To profile, build with `-DTRACY_ENABLE=ON` (or add a project option)
  and connect the Tracy GUI while the game runs.

## Normal build (developer machine with GPU and X11/GL dev packages)

```bash
cmake -S . -B build
cmake --build build -j
# run from a dir containing assets/:
cd build && cp bin/ryupt . && ./ryupt
```

All dependencies (SFML, Box2D, ImGui, imgui-sfml, Tracy, fmt, nlohmann_json)
are fetched by CMake automatically. System prerequisites: a C++20 compiler,
CMake ≥ 3.20, and the SFML system dev libraries (X11, OpenGL/glvnd, udev,
freetype, vorbis/FLAC/openal) plus git network access for FetchContent.

## Recreating the headless sandbox environment (no root, no GPU, no X11)

The agent sandbox has **no root** (no `sudo`, no `apt install`), **no
`/dev/dri`**, and an empty `/tmp` at session start. The recipe below rebuilds
a complete headless build+run environment. It is long but scripted; run the
steps in order.

### 1. apt without root

apt's package lists are unusable without root, but `apt-get` works when given
writable state/cache dirs:

```bash
mkdir -p /tmp/apt/state/lists/partial /tmp/apt/cache/archives/partial \
         /tmp/apt/extended_states /tmp/debs
touch /tmp/apt/extended_states
apt-get -o Dir::State=/tmp/apt/state -o Dir::Cache=/tmp/apt/cache \
  -o Dir::State::extended_states=/tmp/apt/extended_states \
  -o Dir::Etc::sourcelist=/etc/apt/sources.list \
  -o Dir::Etc::sourceparts=/etc/apt/sources.list.d \
  -o APT::Get::List-Cleanup=0 update
```

Define a shorthand for later steps:

```bash
APT="apt-get -o Dir::State=/tmp/apt/state -o Dir::Cache=/tmp/apt/cache \
  -o Dir::State::extended_states=/tmp/apt/extended_states \
  -o Dir::Etc::sourcelist=/etc/apt/sources.list \
  -o Dir::Etc::sourceparts=/etc/apt/sources.list.d"
```

### 2. Download and extract all needed .debs into a local prefix

```bash
cd /tmp/debs
$APT download \
  libx11-dev libx11-6 libxrandr-dev libxrandr2 libxcursor-dev libxcursor1 \
  libxi-dev libxi6 libxext-dev libxext6 libxrender-dev libxrender1 \
  libfreetype-dev libfreetype6 libgl-dev libglx-dev libglx0 libglvnd-dev \
  libegl-dev libudev-dev libxfixes-dev libxfixes3 libxcb1-dev libxcb1 \
  libxau-dev libxau6 libxdmcp-dev libxdmcp6 libbsd-dev libbsd0 libmd-dev \
  libmd0 libbrotli-dev libbrotli1 libpng-dev zlib1g-dev libexpat1-dev \
  libgraphite2-dev libharfbuzz-dev libglib2.0-dev libpcre2-dev x11proto-dev \
  xtrans-dev libopengl-dev libgl1 libopengl0 libegl1 libglvnd0 libglx-mesa0 \
  mesa-libgallium libgl1-mesa-dri libllvm19 libvorbis-dev libvorbis0a \
  libogg-dev libflac-dev libopenal-dev libvorbisfile3 libvorbisenc2 \
  xserver-xorg-core xvfb x11-xkb-utils xkb-data libxfont2 libfontenc1 \
  xfonts-encodings libunwind8 liblzma5 libpixman-1-0 libgcrypt20 libxkbfile1 \
  libxcvt0 libpciaccess0 libdrm2 libgbm1 libwayland-server0 \
  libwayland-client0 libxshmfence1 libxcb-dri3-0 libxcb-present0 \
  libxcb-sync1 libxcb-shm0 libxcb-dri2-0 libxcb-glx0 libxcb-xfixes0 \
  libsensors5 libxcb-randr0 libdrm-amdgpu1 libdrm-intel1 libedit2 libz3-4 \
  libxxf86vm1 libxcomposite1 libxinerama1 x11-utils mesa-utils-bin strace \
  libpng16-16t64 libbrotli1 libgraphite2-3 libharfbuzz0b libpcre2-8-0 \
  libglib2.0-0t64 libbz2-1.0 libx11-xcb1 libelf1t64 libdw1t64

mkdir -p /tmp/prefix /tmp/prefix-x
for f in *.deb; do dpkg -x "$f" /tmp/prefix; done
# runtime lib only present on the system image:
cp /lib/x86_64-linux-gnu/libudev.so.1 /tmp/prefix/usr/lib/x86_64-linux-gnu/
# X server side into a second prefix:
for f in xvfb_* x11-xkb-utils_* xkb-data_* libxfont2_* libfontenc1_* \
          xfonts-encodings_* xserver-xorg-core_*; do
  dpkg -x "$f" /tmp/prefix-x
done
```

Notes:
- Downloading **all** packages in one `apt-get download` call matters: a
  single unknown package name aborts the whole download.
- `mesa-libgallium` is easy to miss and silent-fatal: without
  `libgallium-25.so`, `libGLX_mesa.so.0` fails to dlopen and **every GLX call
  returns 0 fbconfigs** (SFML then crashes with `X Error: BadMatch`).
- If `ldd /tmp/prefix/usr/lib/x86_64-linux-gnu/libGLX_mesa.so.0` shows any
  `not found`, fix it before debugging anything GLX-related.

### 3. Patch Xvfb (hardcoded paths) and start the display

Xvfb hardcodes `/usr/lib/x86_64-linux-gnu/dri` (swrast driver),
`/usr/share/X11/xkb` (keymap data) and `/usr/bin` (xkbcomp) — none writable
without root. Patch the binary to point at `/tmp` equivalents:

```bash
mkdir -p /tmp/dri /tmp/pX /tmp/xkbdata123456
ln -sfn /tmp/prefix/usr/lib/x86_64-linux-gnu/dri/* /tmp/dri/
cp /tmp/prefix-x/usr/bin/xkbcomp /tmp/pX/
ln -sfn /tmp/prefix-x/usr/share/X11/xkb/* /tmp/xkbdata123456/

python3 - <<'EOF'
p = '/tmp/prefix-x/usr/bin/Xvfb'
d = bytearray(open(p, 'rb').read())
def rep(old, new):
    assert d.count(old) == 1, (old, d.count(old))
    assert len(new) <= len(old)
    d.extend(b'')
    i = d.find(old)
    d[i:i+len(new)] = new + b'\x00' * (len(old) - len(new))
rep(b'/usr/lib/x86_64-linux-gnu/dri\x00', b'/tmp/dri\x00')
rep(b'/usr/share/X11/xkb\x00',         b'/tmp/xkbdata123456\x00')
rep(b'/usr/bin\x00',                   b'/tmp/pX\x00')
open(p, 'wb').write(bytes(d))
EOF

export LD_LIBRARY_PATH=/tmp/prefix/usr/lib/x86_64-linux-gnu:/tmp/prefix-x/usr/lib/x86_64-linux-gnu
rm -f /tmp/.X99-lock /tmp/.X11-unix/X99
/tmp/prefix-x/usr/bin/Xvfb :99 -screen 0 1920x1080x24 >/tmp/xvfb.log 2>&1 &
```

Sanity check: `DISPLAY=:99 /tmp/prefix/usr/bin/glxinfo.x86_64-linux-gnu -B`
must report `OpenGL renderer: llvmpipe` and fbconfigs > 0. If it says
"couldn't find RGB GLX visual or fbconfig", a client-side lib is missing
(check `ldd` on `libGLX_mesa.so.0`).

### 4. Configure and build the project

```bash
cmake -S . -B build \
  -DCMAKE_PREFIX_PATH=/tmp/prefix \
  -DCMAKE_LIBRARY_PATH=/tmp/prefix/usr/lib/x86_64-linux-gnu \
  -DCMAKE_INCLUDE_PATH=/tmp/prefix/usr/include \
  -DX11_X11_INCLUDE_PATH=/tmp/prefix/usr/include \
  -DOPENGL_gl_LIBRARY=/tmp/prefix/usr/lib/x86_64-linux-gnu/libGL.so \
  -DOPENGL_opengl_LIBRARY=/tmp/prefix/usr/lib/x86_64-linux-gnu/libOpenGL.so \
  -DOPENGL_glx_LIBRARY=/tmp/prefix/usr/lib/x86_64-linux-gnu/libGLX.so \
  -DUDEV_PATH_LIB=/tmp/prefix/usr/lib/x86_64-linux-gnu \
  -DIMGUI_SFML_FIND_SFML=OFF
cmake --build build -j$(nproc)
```

Important: **always pass the full flag set to `cmake`**. A bare
`cmake -S . -B build` on an existing build dir is fine (cache), but a fresh
configure without the flags fails (FindX11/FindOpenGL) or silently produces a
binary that cannot open a GLX window.

`-DIMGUI_SFML_FIND_SFML=OFF` is required because imgui-sfml calls
`find_package(SFML)`, which cannot see FetchContent targets.

### 5. Run

```bash
cd build
cp -r ../assets .                                   # relative asset paths
cp assets/scenes/99_dummy/grate.png assets/scenes/99_dummy/tile_grate_1.png
cp bin/ryupt .                                      # optional, run from build/
export LD_LIBRARY_PATH=/tmp/prefix/usr/lib/x86_64-linux-gnu:/tmp/prefix-x/usr/lib/x86_64-linux-gnu
export LIBGL_DRIVERS_PATH=/tmp/prefix/usr/lib/x86_64-linux-gnu/dri
DISPLAY=:99 ./ryupt
```

To close the window from a script (verifies clean shutdown; the game must
exit 0 with no `BOX2D ASSERTION`): send `WM_DELETE_WINDOW` via Xlib
(`XSendEvent` with `WM_PROTOCOLS`/`WM_DELETE_WINDOW` to the window whose
`WM_CLASS` is "Ryu prototype 0.1").

### 6. Useful debugging tools in this environment

- `strace` (in prefix): `strace -f -e trace=openat,mmap ./ryupt` to find
  leak sources (e.g. count/size of anonymous mmaps).
- LD_PRELOAD allocation logger: intercept `malloc`/`calloc`/`realloc`/
  `posix_memalign`/`mmap`, log sizes > 1MB with `backtrace()` — this is how
  the Tracy leak was found (all leaked mmaps resolved to `tracy::Profiler`).
- `ps -o %cpu,rss -p <pid>` and `/proc/<pid>/status` for CPU/RSS monitoring.
  Expected steady state: ~60 physics steps/s, ~60 fps, flat RSS ≈ 115MB,
  CPU dominated by the llvmpipe software renderer (~25% in the sandbox;
  near-idle on real hardware).

## Git workflow used in this repo

- Branch: `main`, direct pushes are allowed.
- Fixes go as focused commits with detailed messages (root cause + evidence
  + verification), one logical fix per commit.
- Never push secrets; `build/` and untracked `include/Thirdparty/` content
  are not committed.
