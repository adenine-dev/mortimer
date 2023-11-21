#pragma once

#include "SDL_video.h"
#include "SDL_vulkan.h"
#include <ccVector.h>
#include <stdlib.h>
#include <vulkan/vulkan_core.h>

#include "envlight.h"
#include "imgui_renderer.h"
#include "log.h"
#include "scene.h"
#include "trimesh.h"
#include "types.h"

static const u32 MAX_FRAMES_IN_FLIGHT = 2;

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

typedef struct {
  VkSemaphore image_available;
  VkSemaphore render_finished;
  VkFence in_flight;
  VkCommandBuffer command_buffer;
  VkFramebuffer present_framebuffer;
} PerFrameData;

typedef struct {
  VkImage image;
  VkDeviceMemory memory;
  VkImageView view;
  VkFormat format;
} FramebufferAttachment;

typedef struct {
  VkBuffer handle;
  VkDeviceMemory memory;
} Buffer;

typedef enum : u32 {
  PRESENT_MODE_POSITION = 0,
  PRESENT_MODE_NORMAL = 1,
  PRESENT_MODE_OBJECT_ID = 2,
  PRESENT_MODE_COLOR = 3,
  PRESENT_MODE_ACCUMULATION = 4,
} PresentMode;

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

  FramebufferAttachment depth_attachment;
  FramebufferAttachment position_attachment;
  FramebufferAttachment normal_attachment;
  FramebufferAttachment object_index_attachment;
  FramebufferAttachment trace_output_attachment;
  FramebufferAttachment trace_accumulation_attachment;

  VkSampler vec3_sampler;

  VkCommandPool command_pool;

  VkDescriptorPool descriptor_pool;

  VkRenderPass trace_render_pass;
  VkPipelineLayout first_bounce_pipeline_layout;
  VkPipeline first_bounce_pipeline;
  VkDescriptorSetLayout trace_descriptor_set_layout;
  VkDescriptorSet trace_descriptor_set;
  VkPipelineLayout trace_pipeline_layout;
  VkPipeline trace_pipeline;
  VkDescriptorSetLayout accumulate_descriptor_set_layout;
  VkDescriptorSet accumulate_descriptor_set;
  VkPipelineLayout accumulate_pipeline_layout;
  VkPipeline accumulate_pipeline;

  VkRenderPass present_render_pass;
  VkDescriptorSetLayout present_descriptor_set_layout;
  VkDescriptorSet present_descriptor_set;
  VkPipelineLayout present_pipeline_layout;
  VkPipeline present_pipeline;

  PresentMode present_mode;
  VkRenderPass gui_render_pass;

  Buffer fully_rendered_image_buffer;

  VkFramebuffer *trace_framebuffers;
  VkFramebuffer *swapchain_framebuffers;

  u32 frame;
  PerFrameData frame_data[MAX_FRAMES_IN_FLIGHT];

  mat4x4 camera_view;
  mat4x4 camera_projection;
  f32 camera_fov;
  f32 camera_lens_radius;
  f32 camera_focal_dist;
  u32 accumulated_frames;

  TriangleMesh mesh;
  Buffer vertex_buffer;
  Buffer index_buffer;
  Buffer bvh_buffer;

  u32 material_count;
  Material *materials;
  u32 material_buffer_size;
  Buffer material_buffer;

  EnvironmentLight *envlight;
  VkImage envlight_image;
  VkDeviceMemory envlight_image_memory;
  VkImageView envlight_image_view;

  ImguiRendererImpl imgui_impl;
} Renderer;

Renderer renderer_create(SDL_Window *window);
void renderer_destroy(Renderer *self);
void renderer_update(Renderer *self);
void renderer_resize(Renderer *self, u32 width, u32 height);
void renderer_set_scene(Renderer *self, Scene *scene);

// width, height, image data
typedef void (*ReadFrameCallback)(u32, u32, u8 *);
void renderer_read_frame(Renderer *self, ReadFrameCallback callback);

typedef void (*ReadFrameHdrCallback)(u32, u32, vec4 *);
void renderer_read_frame_hdr(Renderer *self, ReadFrameHdrCallback callback);

#define ASSURE_VK(expr)                                                        \
  {                                                                            \
    VkResult assure_vk_result = expr;                                          \
    if (assure_vk_result != VK_SUCCESS) {                                      \
      errorln("Vulkan expr `%s` failed with code %d\n", #expr,                 \
              assure_vk_result);                                               \
      exit(1);                                                                 \
    }                                                                          \
  }
