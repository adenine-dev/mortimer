set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_definitions("-DIMGUI_DISABLE_OBSOLETE_FUNCTIONS=1")

set(IMGUI_SOURCES cimgui.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_demo.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_tables.cpp
    imgui/backends/imgui_impl_vulkan.cpp
    imgui/backends/imgui_impl_sdl3.cpp
)

if (WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" __declspec\(dllexport\)")
else(WIN32)
    add_definitions("-DIMGUI_IMPL_API=extern \"C\" ")
endif(WIN32)

list(APPEND IMGUI_LIBRARIES Vulkan::Vulkan SDL3::SDL3)

add_library(cimgui_sdl3_vulkan STATIC ${IMGUI_SOURCES})
target_include_directories(cimgui_sdl3_vulkan PUBLIC imgui)
target_include_directories(cimgui_sdl3_vulkan PRIVATE vendor/SDL3/include)
target_link_libraries(cimgui_sdl3_vulkan ${IMGUI_LIBRARIES} SDL3::SDL3)
