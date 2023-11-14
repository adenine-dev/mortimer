# Mortimer 🐁

NOTE: only developed and tested on macos with moltenVK.

## Requires:

- Vulkan SDK
- glslc
- cmake

## build instructions

```sh
git clone https://github.com/adenine-dev/mortimer.git --recurse-submodules
mkdir build
cd build
cmake ..
cd ..
cmake --build ./build && ./build/mortimer
```

## 3rd party

- [SDL](https://github.com/libsdl-org/SDL)
- [ccvector](https://github.com/jobtalle/ccVector)
- [tinyobjloader-c](https://github.com/syoyo/tinyobjloader-c)
- [cimgui](https://github.com/cimgui/cimgui)
- [stb image](https://github.com/nothings/stb)
