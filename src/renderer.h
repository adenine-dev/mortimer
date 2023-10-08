#pragma once

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
  VkImage *swapchain_images;
  VkImageView *swapchain_image_views;
  u32 swapchain_image_count;

  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;

  VkRenderPass render_pass;
  VkPipelineLayout triangle_shader_pipeline_layout;
  VkPipeline triangle_pipeline;

  VkFramebuffer *swapchain_framebuffers;

  VkSemaphore image_available;
  VkSemaphore render_finished;
  VkFence in_flight;
} Renderer;

Renderer renderer_create(SDL_Window *window);

void renderer_destroy(Renderer *);

void renderer_update(Renderer *self);