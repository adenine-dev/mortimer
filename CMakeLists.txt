cmake_minimum_required(VERSION 3.21)

project(
  mortimer
  VERSION 0.0.1
  LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED True)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
add_subdirectory(vendor/SDL3)

add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-braces -Werror)

file(GLOB_RECURSE SOURCES
    ./src/*.h
    ./src/*.c
)

add_executable(mortimer ${SOURCES})
target_include_directories(mortimer PRIVATE ${SDL3_INCLUDE_FILES})
target_link_libraries(mortimer SDL3::SDL3)