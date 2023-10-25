#pragma once

#include <vulkan/vulkan.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

typedef struct Renderer_t Renderer;

typedef struct ImguiRendererImpl_t {
  VkDescriptorPool descriptor_pool;
  ImGui_ImplVulkanH_Window main_window;
} ImguiRendererImpl;

ImguiRendererImpl init_imgui_render_impl(Renderer *renderer,
                                         SDL_Window *window);

void imgui_renderer_update(VkCommandBuffer cmdbuffer);

void imgui_renderer_destroy(Renderer *renderer, ImguiRendererImpl *impl);