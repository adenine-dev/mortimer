#pragma once

#include "SDL_video.h"
#include "SDL_vulkan.h"
#include <ccVector.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include "imgui_renderer.h"
#include "log.h"
#include "types.h"

typedef struct {
  VkPhysicalDevice device;
  u32 graphics_family_index;
  u32 present_family_index;
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkPresentModeKHR present_mode;
  VkSurfaceFormatKHR surface_format;
  VkExtent2D swapchain_extent;
  VkFormat depth_format;

} PhysicalDeviceInfo;

static const u32 MAX_FRAMES_IN_FLIGHT = 2;

typedef struct {
  VkSemaphore image_available;
  VkSemaphore render_finished;
  VkFence in_flight;
  VkCommandBuffer command_buffer;
} PerFrameData;

typedef struct {
  vec3 position;
  vec3 normal;
} Vertex;

typedef struct Renderer_t {
  // direct vulkan stuffs
  VkInstance instance;
  VkDebugUtilsMessengerEXT debug_messenger;
  PhysicalDeviceInfo physical_device_info;
  VkDevice device;
  VkQueue graphics_queue;
  VkQueue present_queue;

  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  u32 swapchain_image_count;
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;

  VkImage depth_image;
  VkDeviceMemory depth_image_memory;
  VkImageView depth_image_view;

  VkCommandPool command_pool;

  VkRenderPass render_pass;
  VkPipelineLayout triangle_shader_pipeline_layout;
  VkPipeline triangle_pipeline;

  VkFramebuffer *swapchain_framebuffers;

  u32 frame;
  PerFrameData frame_data[MAX_FRAMES_IN_FLIGHT];

  mat4x4 camera_view;
  mat4x4 camera_projection;

  u32 vertex_count;
  Vertex *vb;
  u32 index_count;
  u32 *ib;

  VkBuffer vertex_buffer;
  VkDeviceMemory vertex_buffer_memory;
  // VkBuffer index_buffer;
  // VkDeviceMemory index_buffer_memory;

  ImguiRendererImpl imgui_impl;
} Renderer;

Renderer renderer_create(SDL_Window *window);
void renderer_destroy(Renderer *self);
void renderer_update(Renderer *self);
void renderer_resize(Renderer *self, u32 width, u32 height);

void renderer_set_object(Renderer *self, u32 vertex_count, Vertex *vb,
                         u32 index_count, u32 *ib);

#define ASSURE_VK(expr)                                                        \
  {                                                                            \
    VkResult assure_vk_result = expr;                                          \
    if (assure_vk_result != VK_SUCCESS) {                                      \
      errorln("Vulkan expr `%s` failed with code %d\n", #expr,                 \
              assure_vk_result);                                               \
      exit(1);                                                                 \
    }                                                                          \
  }
