cmake_minimum_required(VERSION 3.21)

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include("target_add_binary_embed")
include("add_shader_module")

project(
  mortimer
  VERSION 0.0.1)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
add_subdirectory(vendor/SDL3)

file(GLOB_RECURSE SOURCES
    ./src/*.h
    ./src/*.c
)

find_package(Vulkan REQUIRED)
message(STATUS ${Vulkan_INCLUDE_DIR})

file(GLOB_RECURSE SHADER_MODULES
    # valid glslc extensions
    ./src/shaders/*.vert
    ./src/shaders/*.frag
    ./src/shaders/*.comp
    ./src/shaders/*.geom
    ./src/shaders/*.tesc
    ./src/shaders/*.tese
)

add_subdirectory(vendor/cimgui)

add_executable(mortimer ${SOURCES})
target_compile_options(mortimer PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-braces)

target_compile_definitions(mortimer PUBLIC -DCIMGUI_USE_VULKAN -DCIMGUI_USE_SDL3)
target_include_directories(mortimer PUBLIC vendor/cimgui vendor/cimgui/imgui)
target_include_directories(mortimer PRIVATE vendor/ccVector)
target_include_directories(mortimer PRIVATE vendor/tinyobjloader)
target_include_directories(mortimer PRIVATE ${Vulkan_INCLUDE_DIR})
target_link_libraries(mortimer Vulkan::Vulkan cimgui_sdl3_vulkan)

add_shader_modules(mortimer ${SHADER_MODULES})
