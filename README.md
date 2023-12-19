# Mortimer 🐁

> A realtime-ish path tracer.

NOTE: only developed and tested on macos with moltenVK.

## requires:

- Vulkan SDK
- glslc
- cmake
- clang (other compilers may work but are untested)

## build instructions

```sh
git clone https://github.com/adenine-dev/mortimer.git --recurse-submodules
mkdir build
cd build
cmake ..
cd ..
cmake --build ./build && ./build/mortimer
```

## config

`src/shaders/common/constants.glsl` contains various config options for performance and stylization (requires a recompile)

## 3rd party

- [SDL](https://github.com/libsdl-org/SDL): cross platform window/input.
- [ccvector](https://github.com/jobtalle/ccVector): basic vector maths types.
- [tinyobjloader-c](https://github.com/syoyo/tinyobjloader-c): obj file loading.
- [cimgui](https://github.com/cimgui/cimgui): imgui bindings for C.
- [stb libraries](https://github.com/nothings/stb): image read/write.
- [Fira Code](https://github.com/tonsky/FiraCode): better font for gui.
- See `./assets` directory for various assets.
