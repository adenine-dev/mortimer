#pragma once

#include "SDL_video.h"
#include "SDL_vulkan.h"
#include <vulkan/vulkan_core.h>

#include "types.h"

typedef struct {
  VkPhysicalDevice device;
  u32 graphics_family_index;
  u32 present_family_index;
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkPresentModeKHR present_mode;
  VkSurfaceFormatKHR surface_format;
  VkExtent2D swapchain_extent;
} PhysicalDeviceInfo;

static const u32 MAX_FRAMES_IN_FLIGHT = 2;

typedef struct {
  VkSemaphore image_available;
  VkSemaphore render_finished;
  VkFence in_flight;
  VkCommandBuffer command_buffer;
} PerFrameData;

typedef struct {
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

  VkCommandPool command_pool;

  VkRenderPass render_pass;
  VkPipelineLayout triangle_shader_pipeline_layout;
  VkPipeline triangle_pipeline;

  VkFramebuffer *swapchain_framebuffers;

  u32 frame;
  PerFrameData frame_data[MAX_FRAMES_IN_FLIGHT];
} Renderer;

Renderer renderer_create(SDL_Window *window);
void renderer_destroy(Renderer *self);
void renderer_update(Renderer *self);
void renderer_resize(u32 width, u32 height, Renderer *self);