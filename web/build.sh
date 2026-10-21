#!/bin/sh
# Configure (first run) and build the Emscripten SDL2 frontend into build-wasm/.
set -e
cd "$(dirname "$0")/.."
BUILD=${BUILD:-build-wasm}
if [ ! -f "$BUILD/build.ninja" ]; then
  emcmake cmake -S . -B "$BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=${BUILD_TYPE:-Release} \
    -DENABLE_QT=OFF -DENABLE_SDL2=ON -DENABLE_SDL2_FRONTEND=ON \
    -DENABLE_VULKAN=OFF -DENABLE_OPENGL=ON -DENABLE_SOFTWARE_RENDERER=ON \
    -DENABLE_WEB_SERVICE=OFF -DENABLE_SCRIPTING=OFF -DENABLE_GDBSTUB=OFF \
    -DENABLE_CUBEB=OFF -DENABLE_OPENAL=OFF -DENABLE_LIBUSB=OFF \
    -DENABLE_TESTS=OFF -DENABLE_ROOM=OFF -DENABLE_ROOM_STANDALONE=OFF \
    -DENABLE_LTO=OFF -DENABLE_DISCORD_RPC=OFF -DENABLE_MICROPROFILE=OFF \
    -DCITRA_USE_PRECOMPILED_HEADERS=OFF -DCITRA_WARNINGS_AS_ERRORS=OFF \
    -DENABLE_GENERIC=ON -DSDL_RENDER=ON \
    -DCMAKE_C_FLAGS=-pthread "-DCMAKE_CXX_FLAGS=-pthread -Wno-c++11-narrowing"
fi
ninja -C "$BUILD" -k ${KEEP:-0} ${TARGET:-citra_sdl}
mkdir -p web/out
cp "$BUILD"/bin/*/azahar.* "$BUILD"/src/citra_sdl/azahar.* web/out/ 2>/dev/null || true
