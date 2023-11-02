#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL_video.h"
#include "ccVector.h"
#include <vulkan/vulkan_core.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "imgui_renderer.h"
#include "log.h"
#include "maths.h"
#include "renderer.h"
#include "types.h"

#include "shaders/embed/first_bounce_frag_spv.h"
#include "shaders/embed/first_bounce_vert_spv.h"
#include "shaders/embed/fullscreen_quad_vert_spv.h"
#include "shaders/embed/pathtrace_frag_spv.h"
#include "shaders/embed/present_frag_spv.h"

bool assure_validation_layer_support(usize n, const char **requested_layers) {
  u32 available_layer_count;
  ASSURE_VK(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));
  VkLayerProperties *available_layers = (VkLayerProperties *)malloc(
      (usize)available_layer_count * sizeof(VkLayerProperties));
  ASSURE_VK(vkEnumerateInstanceLayerProperties(&available_layer_count,
                                               available_layers));

  bool all_found = true;
  for (u32 i = 0; i < n; ++i) {
    bool found = false;
    for (usize j = 0; j < available_layer_count; ++j) {
      if (strcmp(requested_layers[i], available_layers[j].layerName) == 0) {
        found = true;
        break;
      }
    }

    if (!found) {
      all_found = false;
    }
  }

  free(available_layers);
  return all_found;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                      VkDebugUtilsMessageTypeFlagsEXT messageType,
                      const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                      void *pUserData) {

  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
    printf("\033[34m[VK INFO]:\033[0m %s \n", pCallbackData->pMessage);
  }
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    printf("\033[33m[VK WARN]:\033[0m %s \n", pCallbackData->pMessage);
  }
  if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
    printf("\033[31m[VK ERROR]:\033[0m %s \n", pCallbackData->pMessage);
  }

  return VK_FALSE;
}

static const u32 REQUIRED_DEVICE_EXTENSION_COUNT = 2;
static const char *REQUIRED_DEVICE_EXTENSIONS[REQUIRED_DEVICE_EXTENSION_COUNT] =
    {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        "VK_KHR_portability_subset",
};

void find_swapchain_extent(SDL_Window *window,
                           PhysicalDeviceInfo *device_info) {
  if (device_info->surface_capabilities.currentExtent.width != UINT32_MAX) {
    device_info->swapchain_extent =
        device_info->surface_capabilities.currentExtent;
  } else {
    int width, height;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    device_info->swapchain_extent = (VkExtent2D){
        .width = clamp((u32)width,
                       device_info->surface_capabilities.minImageExtent.width,
                       device_info->surface_capabilities.maxImageExtent.width),
        .height =
            clamp((u32)height,
                  device_info->surface_capabilities.minImageExtent.height,
                  device_info->surface_capabilities.maxImageExtent.height),
    };
  }
}

const usize ACCEPTABLE_DEPTH_FORMAT_COUNT = 3;
const VkFormat ACCEPTABLE_DEPTH_FORMATS[ACCEPTABLE_DEPTH_FORMAT_COUNT] = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
};

PhysicalDeviceInfo get_physical_device(VkInstance instance, SDL_Window *window,
                                       VkSurfaceKHR surface) {
  u32 device_count;
  vkEnumeratePhysicalDevices(instance, &device_count, NULL);
  VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
  vkEnumeratePhysicalDevices(instance, &device_count, devices);

  PhysicalDeviceInfo chosen = (PhysicalDeviceInfo){
      .device = VK_NULL_HANDLE,
      .graphics_family_index = UINT32_MAX,
      .present_family_index = UINT32_MAX,
      .depth_format = VK_FORMAT_UNDEFINED,
  };

  for (u32 i = 0; i < device_count; i++) {
    PhysicalDeviceInfo device_info = (PhysicalDeviceInfo){
        .device = devices[i],
        .graphics_family_index = UINT32_MAX,
        .present_family_index = UINT32_MAX,
    };
    VkPhysicalDeviceProperties device_props;
    vkGetPhysicalDeviceProperties(device_info.device, &device_props);
    VkPhysicalDeviceFeatures device_features;
    vkGetPhysicalDeviceFeatures(device_info.device, &device_features);

    u32 extension_count;
    vkEnumerateDeviceExtensionProperties(device_info.device, NULL,
                                         &extension_count, NULL);
    VkExtensionProperties *extension_props = (VkExtensionProperties *)malloc(
        extension_count * sizeof(VkExtensionProperties));
    vkEnumerateDeviceExtensionProperties(device_info.device, NULL,
                                         &extension_count, extension_props);

    bool found_extension[REQUIRED_DEVICE_EXTENSION_COUNT] = {0};
    for (u32 i = 0; i < REQUIRED_DEVICE_EXTENSION_COUNT; ++i) {
      for (u32 j = 0; j < extension_count; ++j) {
        if (strcmp(REQUIRED_DEVICE_EXTENSIONS[i],
                   extension_props[j].extensionName) == 0) {
          found_extension[i] = true;
          break;
        }
      }
    }

    bool found_all_required_extensions = true;
    for (u32 i = 0; i < REQUIRED_DEVICE_EXTENSION_COUNT; ++i) {
      if (!found_extension[i]) {
        found_all_required_extensions = false;
        break;
      }
    }

    if (!found_all_required_extensions) {
      free(extension_props);
      continue;
    }

    // find surface format
    {
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          device_info.device, surface, &device_info.surface_capabilities);
      u32 format_count;
      vkGetPhysicalDeviceSurfaceFormatsKHR(device_info.device, surface,
                                           &format_count, NULL);
      if (format_count == 0) {
        continue;
      }
      VkSurfaceFormatKHR *formats =
          malloc(format_count * sizeof(VkSurfaceFormatKHR));
      vkGetPhysicalDeviceSurfaceFormatsKHR(device_info.device, surface,
                                           &format_count, formats);
      device_info.surface_format = formats[0];
      for (u32 i = 0; i < format_count; ++i) {
        if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
            formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          device_info.surface_format = formats[i];
          break;
        }
      }

      free(formats);
    }

    // find depth format
    {
      VkFormatFeatureFlags feature_flags =
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

      for (usize i = 0; i < ACCEPTABLE_DEPTH_FORMAT_COUNT; ++i) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(
            device_info.device, ACCEPTABLE_DEPTH_FORMATS[i], &props);
        if ((props.optimalTilingFeatures & feature_flags) == feature_flags) {
          device_info.depth_format = ACCEPTABLE_DEPTH_FORMATS[i];
          break;
        }
      }

      if (device_info.depth_format == VK_FORMAT_UNDEFINED)
        continue;
    }

    // find present mode
    u32 present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device_info.device, surface,
                                              &present_mode_count, NULL);
    if (present_mode_count == 0) {
      continue;
    }
    VkPresentModeKHR *present_modes =
        malloc(present_mode_count * sizeof(VkPresentModeKHR));
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device_info.device, surface, &present_mode_count, present_modes);
    device_info.present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < present_mode_count; ++i) {
      device_info.present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    }

    // get queue families
    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device_info.device,
                                             &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families =
        malloc(queue_family_count * sizeof(VkQueueFamilyProperties));

    vkGetPhysicalDeviceQueueFamilyProperties(
        device_info.device, &queue_family_count, queue_families);

    for (u32 i = 0; i < queue_family_count; i++) {
      if (device_info.graphics_family_index == UINT32_MAX &&
          queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        device_info.graphics_family_index = i;
      }

      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(device_info.device, i, surface,
                                           &presentSupport);
      if (presentSupport) {
        device_info.present_family_index = i;
      }
    }

    // get extent
    find_swapchain_extent(window, &device_info);

    free(present_modes);
    free(queue_families);
    free(extension_props);

    // very simple here we just want to get the first gpu that satisfies all
    // requirements, room for more here in future but yknow
    if (device_info.graphics_family_index != UINT32_MAX &&
        device_info.present_family_index != UINT32_MAX) {
      chosen = device_info;
      break;
    }
  }

  if (chosen.device == VK_NULL_HANDLE) {
    errorln("could not find suitable gpu");
    exit(1);
  }

  VkPhysicalDeviceProperties device_props;
  vkGetPhysicalDeviceProperties(chosen.device, &device_props);

  infoln("using gpu: %s", device_props.deviceName);

  free(devices);

  return chosen;
}

u32 find_memory_type(Renderer *self, u32 type_filter,
                     VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(self->physical_device_info.device,
                                      &mem_props);

  for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  fatalln("could not find usable memory type");
}

VkShaderModule create_shader_module(VkDevice device, const unsigned char *spv,
                                    usize spv_len) {
  VkShaderModuleCreateInfo create_info = (VkShaderModuleCreateInfo){
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .codeSize = spv_len,
      .pCode = (u32 *)spv,
  };

  VkShaderModule shader_module;
  ASSURE_VK(vkCreateShaderModule(device, &create_info, NULL, &shader_module));

  return shader_module;
}

FramebufferAttachment
create_framebuffer_attachment(Renderer *renderer, VkFormat format,
                              VkImageUsageFlagBits usage) {
  FramebufferAttachment fba = {0};
  fba.format = format;
  VkImageCreateInfo image_create_info = (VkImageCreateInfo){
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format,
      .extent =
          (VkExtent3D){
              .width = renderer->physical_device_info.swapchain_extent.width,
              .height = renderer->physical_device_info.swapchain_extent.height,
              .depth = 1,
          },
      .mipLevels = 1,
      .arrayLayers = 1,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .usage = usage,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  ASSURE_VK(
      vkCreateImage(renderer->device, &image_create_info, NULL, &fba.image));
  VkMemoryRequirements image_mem_reqs;
  vkGetImageMemoryRequirements(renderer->device, fba.image, &image_mem_reqs);
  VkMemoryAllocateInfo image_mem_allocate_info = (VkMemoryAllocateInfo){
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = image_mem_reqs.size,
      .memoryTypeIndex =
          find_memory_type(renderer, image_mem_reqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };
  vkAllocateMemory(renderer->device, &image_mem_allocate_info, NULL,
                   &fba.memory);
  vkBindImageMemory(renderer->device, fba.image, fba.memory, 0);

  VkImageAspectFlags aspect_mask = 0;
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
  } else if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (format >= VK_FORMAT_D16_UNORM_S8_UINT) {
      aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
  }

  VkImageViewCreateInfo image_view_create_info = (VkImageViewCreateInfo){
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = fba.image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components =
          (VkComponentMapping){
              .r = VK_COMPONENT_SWIZZLE_IDENTITY,
              .g = VK_COMPONENT_SWIZZLE_IDENTITY,
              .b = VK_COMPONENT_SWIZZLE_IDENTITY,
              .a = VK_COMPONENT_SWIZZLE_IDENTITY,
          },
      .subresourceRange =
          (VkImageSubresourceRange){
              .aspectMask = aspect_mask,
              .baseMipLevel = 0,
              .levelCount = 1,
              .baseArrayLayer = 0,
              .layerCount = 1,
          },
  };
  ASSURE_VK(vkCreateImageView(renderer->device, &image_view_create_info, NULL,
                              &fba.view));

  return fba;
}

void create_swapchain(Renderer *self) {
  VkSwapchainCreateInfoKHR swapchain_create_info = (VkSwapchainCreateInfoKHR){
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = self->surface,
      .minImageCount = self->swapchain_image_count,
      .imageFormat = self->physical_device_info.surface_format.format,
      .imageColorSpace = self->physical_device_info.surface_format.colorSpace,
      .imageExtent = self->physical_device_info.swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform =
          self->physical_device_info.surface_capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = self->physical_device_info.present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  u32 queue_family_indicies[2] = {
      self->physical_device_info.graphics_family_index,
      self->physical_device_info.present_family_index,
  };
  if (self->physical_device_info.graphics_family_index !=
      self->physical_device_info.present_family_index) {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount = 2;

    swapchain_create_info.pQueueFamilyIndices = queue_family_indicies;
  }

  ASSURE_VK(vkCreateSwapchainKHR(self->device, &swapchain_create_info, NULL,
                                 &self->swapchain));

  vkGetSwapchainImagesKHR(self->device, self->swapchain,
                          &self->swapchain_image_count, NULL);

  if (!self->swapchain_images) {
    self->swapchain_images =
        malloc(sizeof(VkImage) * self->swapchain_image_count);
  }
  vkGetSwapchainImagesKHR(self->device, self->swapchain,
                          &self->swapchain_image_count, self->swapchain_images);

  if (!self->swapchain_image_views) {
    self->swapchain_image_views =
        malloc(sizeof(VkImageView) * self->swapchain_image_count);
  }

  VkImageViewCreateInfo swapchain_image_view_create_info =
      (VkImageViewCreateInfo){
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = self->physical_device_info.surface_format.format,
          .components =
              (VkComponentMapping){
                  .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                  .a = VK_COMPONENT_SWIZZLE_IDENTITY,
              },
          .subresourceRange =
              (VkImageSubresourceRange){
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = 0,
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };

  for (u32 i = 0; i < self->swapchain_image_count; i++) {
    swapchain_image_view_create_info.image = self->swapchain_images[i];

    ASSURE_VK(vkCreateImageView(self->device, &swapchain_image_view_create_info,
                                NULL, &self->swapchain_image_views[i]));
  }

  self->depth_attachment = create_framebuffer_attachment(
      self, self->physical_device_info.depth_format,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

  self->normal_attachment = create_framebuffer_attachment(
      self, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
  self->position_attachment = create_framebuffer_attachment(
      self, VK_FORMAT_R16G16B16A16_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

  self->trace_output_attachment = create_framebuffer_attachment(
      self, VK_FORMAT_R32G32B32A32_SFLOAT,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
}

void create_framebuffers(Renderer *self) {
  if (!self->swapchain_framebuffers) {
    self->swapchain_framebuffers =
        malloc(self->swapchain_image_count * sizeof(VkFramebuffer));
  }
  if (!self->trace_framebuffers) {
    self->trace_framebuffers =
        malloc(self->swapchain_image_count * sizeof(VkFramebuffer));
  }

  for (u32 i = 0; i < self->swapchain_image_count; i++) {
    // trace framebuffer
    {
      const u32 attachment_count = 4;
      VkImageView attachments[attachment_count] = {
          self->trace_output_attachment.view,
          self->position_attachment.view,
          self->normal_attachment.view,
          self->depth_attachment.view,
      };

      VkFramebufferCreateInfo framebuffer_create_info =
          (VkFramebufferCreateInfo){
              .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              .renderPass = self->trace_render_pass,
              .attachmentCount = attachment_count,
              .pAttachments = attachments,
              .width = self->physical_device_info.swapchain_extent.width,
              .height = self->physical_device_info.swapchain_extent.height,
              .layers = 1,
          };

      ASSURE_VK(vkCreateFramebuffer(self->device, &framebuffer_create_info,
                                    NULL, &self->trace_framebuffers[i]));
    }

    // present/swapchain framebuffer
    {
      const u32 attachment_count = 1;
      VkImageView attachments[attachment_count] = {
          self->swapchain_image_views[i],
      };

      VkFramebufferCreateInfo framebuffer_create_info =
          (VkFramebufferCreateInfo){
              .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
              .renderPass = self->present_render_pass,
              .attachmentCount = attachment_count,
              .pAttachments = attachments,
              .width = self->physical_device_info.swapchain_extent.width,
              .height = self->physical_device_info.swapchain_extent.height,
              .layers = 1,
          };

      ASSURE_VK(vkCreateFramebuffer(self->device, &framebuffer_create_info,
                                    NULL, &self->swapchain_framebuffers[i]));
    }
  }
}

void renderer_set_or_update_camera(Renderer *self) {
  mat4x4Perspective(self->camera_projection, self->camera_fov * (M_PI / 180.0),
                    (f32)self->physical_device_info.swapchain_extent.width /
                        (f32)self->physical_device_info.swapchain_extent.height,
                    0.001, 1000.0);
}

typedef struct {
  mat4x4 camera_matrix;
} FirstBouncePushConstants;

typedef struct {
  u32 mode;
} PresentPushConstants;

enum PresentMode {
  PRESENT_MODE_COLOR = 0,
  PRESENT_MODE_NORMAL = 1,
};

const u32 VALIDATION_LAYER_COUNT = 1;
const char *VALIDATION_LAYERS[VALIDATION_LAYER_COUNT] = {
    "VK_LAYER_KHRONOS_validation",
};

Renderer renderer_create(SDL_Window *window) {
  Renderer renderer = {0};

  bool enable_validation_layers = true;
  { // create instance & debug stuffs
    unsigned int extension_count;
    if (!SDL_Vulkan_GetInstanceExtensions(&extension_count, NULL)) {
      errorln("failed to get number of vulkan instance extensions from SDL");
      exit(1);
    }
    const unsigned int extra_extensions = 2;
    const char **extensions = (const char **)malloc(
        (extension_count + extra_extensions) * sizeof(const char *));

    if (!SDL_Vulkan_GetInstanceExtensions(&extension_count, extensions)) {
      errorln("failed to get vulkan instance extensions from SDL");
      exit(1);
    }

    extensions[extension_count] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
    extensions[extension_count + 1] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    extension_count += extra_extensions;

    if (!assure_validation_layer_support(1, VALIDATION_LAYERS)) {
      warnln("could not get all validation layers requested.\n");
      enable_validation_layers = false;
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_utils_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vulkan_debug_callback,
    };

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = (void *)&debug_utils_create_info,
        .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
        .pApplicationInfo =
            &(VkApplicationInfo){
                .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                .pApplicationName = "Mortimer",
                .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
                .pEngineName = "Mortimer",
                .engineVersion = VK_MAKE_VERSION(0, 1, 0),
                .apiVersion = VK_API_VERSION_1_1,
            },
    };

    if (enable_validation_layers) {
      create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
      create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    }

    ASSURE_VK(vkCreateInstance(&create_info, NULL, &renderer.instance));

    // create debug messenger
    renderer.debug_messenger = VK_NULL_HANDLE;
    if (enable_validation_layers) {
      PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
          (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
              renderer.instance, "vkCreateDebugUtilsMessengerEXT");
      if (vkCreateDebugUtilsMessengerEXT != NULL) {
        ASSURE_VK(vkCreateDebugUtilsMessengerEXT(renderer.instance,
                                                 &debug_utils_create_info, NULL,
                                                 &renderer.debug_messenger));
      } else {
        warnln("could not create debug util messenger");
      }
    }

    free(extensions);
  }

  // create surface
  SDL_Vulkan_CreateSurface(window, renderer.instance, &renderer.surface);

  { // create device & queues
    renderer.physical_device_info =
        get_physical_device(renderer.instance, window, renderer.surface);

    // create logical device
    f32 queue_priority = 1.0;
    const u32 queue_create_info_count =
        renderer.physical_device_info.graphics_family_index ==
                renderer.physical_device_info.present_family_index
            ? 1
            : 2;
    VkDeviceQueueCreateInfo queue_create_infos[queue_create_info_count];
    if (renderer.physical_device_info.graphics_family_index ==
        renderer.physical_device_info.present_family_index) {
      queue_create_infos[0] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex =
              renderer.physical_device_info.graphics_family_index,
          .queueCount = 1,
          .pQueuePriorities = &queue_priority,
      };
    } else {
      queue_create_infos[0] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex =
              renderer.physical_device_info.graphics_family_index,
          .queueCount = 1,
          .pQueuePriorities = &queue_priority,
      };
      queue_create_infos[1] = (VkDeviceQueueCreateInfo){
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex =
              renderer.physical_device_info.present_family_index,
          .queueCount = 1,
          .pQueuePriorities = &queue_priority,
      };
    }

    VkPhysicalDeviceFeatures enabled_features = {};
    const u32 device_extension_count = REQUIRED_DEVICE_EXTENSION_COUNT;
    const char **device_extensions =
        (const char **)malloc(device_extension_count * sizeof(const char *));
    memcpy(device_extensions, REQUIRED_DEVICE_EXTENSIONS,
           REQUIRED_DEVICE_EXTENSION_COUNT * sizeof(const char *));

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = queue_create_info_count,
        .pEnabledFeatures = &enabled_features,
        .enabledExtensionCount = device_extension_count,
        .ppEnabledExtensionNames = device_extensions,
    };

    if (enable_validation_layers) {
      device_create_info.enabledLayerCount = VALIDATION_LAYER_COUNT;
      device_create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
    }

    ASSURE_VK(vkCreateDevice(renderer.physical_device_info.device,
                             &device_create_info, NULL, &renderer.device));

    vkGetDeviceQueue(renderer.device,
                     renderer.physical_device_info.graphics_family_index, 0,
                     &renderer.graphics_queue);

    vkGetDeviceQueue(renderer.device,
                     renderer.physical_device_info.present_family_index, 0,
                     &renderer.present_queue);

    free(device_extensions);
  }

  { // create swapchain
    renderer.swapchain_image_count =
        renderer.physical_device_info.surface_capabilities.minImageCount + 1;
    if (renderer.physical_device_info.surface_capabilities.maxImageCount != 0 &&
        renderer.swapchain_image_count >
            renderer.physical_device_info.surface_capabilities.maxImageCount) {
      renderer.swapchain_image_count =
          renderer.physical_device_info.surface_capabilities.maxImageCount;
    }

    create_swapchain(&renderer);
  }

  VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];
  { // create command buffers
    VkCommandPoolCreateInfo command_pool_create_info =
        (VkCommandPoolCreateInfo){
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex =
                renderer.physical_device_info.graphics_family_index,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        };

    ASSURE_VK(vkCreateCommandPool(renderer.device, &command_pool_create_info,
                                  NULL, &renderer.command_pool));

    VkCommandBufferAllocateInfo command_buffer_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = renderer.command_pool,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    vkAllocateCommandBuffers(renderer.device, &command_buffer_alloc_info,
                             command_buffers);
  }

  { // create attachments and trace renderpass
    VkAttachmentDescription color_attachment_desc = {
        .format = renderer.trace_output_attachment.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentDescription position_attachment_desc = {
        .format = renderer.position_attachment.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentDescription normal_attachment_desc = {
        .format = renderer.normal_attachment.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkAttachmentDescription depth_attachment_desc = {
        .format = renderer.physical_device_info.depth_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    VkAttachmentReference depth_attachment_ref = {
        .attachment = 3,
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };

    const u32 first_bounce_attachment_count = 2;
    VkAttachmentReference
        first_bounce_subpass_attachment_refs[first_bounce_attachment_count] = {
            {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, // position
            {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, // normal
        };

    const u32 trace_attachment_count = 1;
    VkAttachmentReference
        trace_subpass_attachment_refs[trace_attachment_count] = {
            {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        };
    VkAttachmentReference
        trace_subpass_input_attachment_refs[first_bounce_attachment_count] = {
            {1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, // position
            {2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, // normal
        };
    const u32 subpass_desc_count = 2;
    VkSubpassDescription subpass_descs[subpass_desc_count] = {
        (VkSubpassDescription){
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = first_bounce_attachment_count,
            .pColorAttachments = first_bounce_subpass_attachment_refs,
            .pDepthStencilAttachment = &depth_attachment_ref,
        },
        (VkSubpassDescription){
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .inputAttachmentCount = first_bounce_attachment_count,
            .pInputAttachments = trace_subpass_input_attachment_refs,
            .colorAttachmentCount = trace_attachment_count,
            .pColorAttachments = trace_subpass_attachment_refs,
        },
    };

    const u32 render_pass_dependency_count = 3;
    VkSubpassDependency render_pass_dependencies[render_pass_dependency_count] =
        {
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            {
                .srcSubpass = 0,
                .dstSubpass = 1,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            {
                .srcSubpass = 1,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
        };

    const u32 attachment_desc_count = 4;
    VkAttachmentDescription attachment_descs[attachment_desc_count] = {
        color_attachment_desc,
        position_attachment_desc,
        normal_attachment_desc,
        depth_attachment_desc,
    };
    VkRenderPassCreateInfo render_pass_create_info = (VkRenderPassCreateInfo){
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachment_desc_count,
        .pAttachments = attachment_descs,
        .subpassCount = subpass_desc_count,
        .pSubpasses = subpass_descs,
        .dependencyCount = render_pass_dependency_count,
        .pDependencies = render_pass_dependencies,
    };
    ASSURE_VK(vkCreateRenderPass(renderer.device, &render_pass_create_info,
                                 NULL, &renderer.trace_render_pass))
  }

  { // create attachments and present renderpass
    VkAttachmentDescription swapchain_attachment_desc = {
        .format = renderer.physical_device_info.surface_format.format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference swapchain_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    const u32 present_attachment_count = 1;
    VkAttachmentReference
        present_subpass_attachment_refs[present_attachment_count] = {
            {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        };
    const u32 subpass_desc_count = 1;
    VkSubpassDescription subpass_descs[subpass_desc_count] = {
        (VkSubpassDescription){
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .colorAttachmentCount = present_attachment_count,
            .pColorAttachments = present_subpass_attachment_refs,
        },
    };

    const u32 render_pass_dependency_count = 2;
    VkSubpassDependency render_pass_dependencies[render_pass_dependency_count] =
        {
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
            {
                .srcSubpass = 0,
                .dstSubpass = VK_SUBPASS_EXTERNAL,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
                .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
            },
        };

    const u32 attachment_desc_count = 1;
    VkAttachmentDescription attachment_descs[attachment_desc_count] = {
        swapchain_attachment_desc,
    };
    VkRenderPassCreateInfo render_pass_create_info = (VkRenderPassCreateInfo){
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = attachment_desc_count,
        .pAttachments = attachment_descs,
        .subpassCount = subpass_desc_count,
        .pSubpasses = subpass_descs,
        .dependencyCount = render_pass_dependency_count,
        .pDependencies = render_pass_dependencies,
    };
    ASSURE_VK(vkCreateRenderPass(renderer.device, &render_pass_create_info,
                                 NULL, &renderer.present_render_pass))
  }

  { // create samplers
    VkSamplerCreateInfo vec3_sampler_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
        .maxAnisotropy = 1.0,
        .minLod = 0.0f,
        .maxLod = 1.0f,
    };

    vkCreateSampler(renderer.device, &vec3_sampler_create_info, NULL,
                    &renderer.vec3_sampler);
  }

  { // create descriptor pool
    const u32 pool_sizes_len = 2;
    VkDescriptorPoolSize pool_sizes[pool_sizes_len] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 2},
    };

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
        .poolSizeCount = pool_sizes_len,
        .pPoolSizes = pool_sizes,
    };

    ASSURE_VK(vkCreateDescriptorPool(renderer.device,
                                     &descriptor_pool_create_info, NULL,
                                     &renderer.descriptor_pool));
  }

  { // first bounce pipeline
    VkShaderModule first_bounce_vert_shader =
        create_shader_module(renderer.device, first_bounce_vert_spv_data,
                             first_bounce_vert_spv_size);
    VkShaderModule first_bounce_frag_shader =
        create_shader_module(renderer.device, first_bounce_frag_spv_data,
                             first_bounce_frag_spv_size);

    VkPipelineShaderStageCreateInfo first_bounce_shader_stage_create_infos[] = {
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = first_bounce_vert_shader,
            .pName = "main",
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = first_bounce_frag_shader,
            .pName = "main",
        },
    };

    const u32 dynamic_state_count = 2;
    const VkDynamicState dynamic_states[dynamic_state_count] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineLayoutCreateInfo
        first_bounce_shader_pipeline_layout_create_info =
            (VkPipelineLayoutCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges =
                    &(VkPushConstantRange){
                        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                        .offset = 0,
                        .size = sizeof(FirstBouncePushConstants),
                    },
            };

    ASSURE_VK(vkCreatePipelineLayout(
        renderer.device, &first_bounce_shader_pipeline_layout_create_info, NULL,
        &renderer.first_bounce_pipeline_layout));

    VkVertexInputBindingDescription first_bounce_vertex_input_binding_desc =
        (VkVertexInputBindingDescription){
            .binding = 0,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            .stride = sizeof(Vertex),
        };
    const u32 first_bounce_vertex_input_attr_desc_count = 2;
    VkVertexInputAttributeDescription first_bounce_vertex_input_attr_descs
        [first_bounce_vertex_input_attr_desc_count] = {
            (VkVertexInputAttributeDescription){
                .binding = 0,
                .location = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, position),
            },
            (VkVertexInputAttributeDescription){
                .binding = 0,
                .location = 1,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, normal),
            },
        };
    VkPipelineColorBlendAttachmentState blend_attachment_states[] = {
        (VkPipelineColorBlendAttachmentState){
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable = VK_FALSE,
        },
        (VkPipelineColorBlendAttachmentState){
            .colorWriteMask =
                VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable = VK_FALSE,
        },
    };
    VkGraphicsPipelineCreateInfo first_bounce_pipeline_create_info =
        (VkGraphicsPipelineCreateInfo){
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = first_bounce_shader_stage_create_infos,
            .layout = renderer.first_bounce_pipeline_layout,
            .renderPass = renderer.trace_render_pass,
            .subpass = 0,
            .pVertexInputState =
                &(VkPipelineVertexInputStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    .vertexBindingDescriptionCount = 1,
                    .pVertexBindingDescriptions =
                        &first_bounce_vertex_input_binding_desc,
                    .vertexAttributeDescriptionCount =
                        first_bounce_vertex_input_attr_desc_count,
                    .pVertexAttributeDescriptions =
                        first_bounce_vertex_input_attr_descs,
                },
            .pInputAssemblyState =
                &(VkPipelineInputAssemblyStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    .primitiveRestartEnable = VK_FALSE,
                },
            .pViewportState =
                &(VkPipelineViewportStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .scissorCount = 1,
                },
            .pRasterizationState =
                &(VkPipelineRasterizationStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_BACK_BIT,
                    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                    .lineWidth = 1.0,
                },
            .pMultisampleState =
                &(VkPipelineMultisampleStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .sampleShadingEnable = VK_FALSE,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                },
            .pDepthStencilState =
                &(VkPipelineDepthStencilStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = VK_TRUE,
                    .depthWriteEnable = VK_TRUE,
                    .depthCompareOp = VK_COMPARE_OP_LESS,
                    .depthBoundsTestEnable = VK_FALSE,
                    .minDepthBounds = 0.0f,
                    .maxDepthBounds = 1.0f,
                    .stencilTestEnable = VK_TRUE,
                },
            .pColorBlendState =
                &(VkPipelineColorBlendStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = VK_FALSE,
                    // .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = 2,
                    .pAttachments = blend_attachment_states,
                },
            .pDynamicState =
                &(VkPipelineDynamicStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = dynamic_state_count,
                    .pDynamicStates = dynamic_states,
                },
        };

    vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1,
                              &first_bounce_pipeline_create_info, NULL,
                              &renderer.first_bounce_pipeline);

    vkDestroyShaderModule(renderer.device, first_bounce_frag_shader, NULL);
    vkDestroyShaderModule(renderer.device, first_bounce_vert_shader, NULL);
  }

  { // path trace pipeline
    const u32 binding_count = 2;
    VkDescriptorSetLayoutBinding bindings[binding_count] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo trace_descriptor_set_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = binding_count,
        .pBindings = bindings,
    };

    ASSURE_VK(vkCreateDescriptorSetLayout(
        renderer.device, &trace_descriptor_set_layout_create_info, NULL,
        &renderer.trace_descriptor_set_layout));

    VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = renderer.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &renderer.trace_descriptor_set_layout,
    };
    ASSURE_VK(vkAllocateDescriptorSets(renderer.device,
                                       &descriptor_set_alloc_info,
                                       &renderer.trace_descriptor_set));

    VkShaderModule trace_vert_shader =
        create_shader_module(renderer.device, fullscreen_quad_vert_spv_data,
                             fullscreen_quad_vert_spv_size);
    VkShaderModule trace_frag_shader = create_shader_module(
        renderer.device, pathtrace_frag_spv_data, pathtrace_frag_spv_size);

    VkPipelineShaderStageCreateInfo trace_shader_stage_create_infos[] = {
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = trace_vert_shader,
            .pName = "main",
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = trace_frag_shader,
            .pName = "main",
        },
    };

    const u32 dynamic_state_count = 2;
    const VkDynamicState dynamic_states[dynamic_state_count] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineLayoutCreateInfo trace_shader_pipeline_layout_create_info =
        (VkPipelineLayoutCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &renderer.trace_descriptor_set_layout,
            // .pushConstantRangeCount = 1,
            // .pPushConstantRanges =
            //     &(VkPushConstantRange){
            //         .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            //         .offset = 0,
            //         .size = sizeof(FirstBouncePushConstants),
            //     },
        };

    ASSURE_VK(vkCreatePipelineLayout(renderer.device,
                                     &trace_shader_pipeline_layout_create_info,
                                     NULL, &renderer.trace_pipeline_layout));

    VkVertexInputBindingDescription trace_vertex_input_binding_desc =
        (VkVertexInputBindingDescription){
            .binding = 0,
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
            .stride = sizeof(Vertex),
        };

    VkGraphicsPipelineCreateInfo trace_pipeline_create_info =
        (VkGraphicsPipelineCreateInfo){
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = trace_shader_stage_create_infos,
            .layout = renderer.trace_pipeline_layout,
            .renderPass = renderer.trace_render_pass,
            .subpass = 1,
            .pVertexInputState =
                &(VkPipelineVertexInputStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    .vertexAttributeDescriptionCount = 0,
                    .pVertexAttributeDescriptions = NULL,
                },
            .pInputAssemblyState =
                &(VkPipelineInputAssemblyStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    .primitiveRestartEnable = VK_FALSE,
                },
            .pViewportState =
                &(VkPipelineViewportStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .scissorCount = 1,
                },
            .pRasterizationState =
                &(VkPipelineRasterizationStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_FRONT_BIT,
                    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                    .lineWidth = 1.0,
                },
            .pMultisampleState =
                &(VkPipelineMultisampleStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .sampleShadingEnable = VK_FALSE,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                },
            .pDepthStencilState =
                &(VkPipelineDepthStencilStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = VK_TRUE,
                    .depthWriteEnable = VK_TRUE,
                    .depthCompareOp = VK_COMPARE_OP_LESS,
                    .depthBoundsTestEnable = VK_FALSE,
                    .minDepthBounds = 0.0f,
                    .maxDepthBounds = 1.0f,
                    .stencilTestEnable = VK_TRUE,
                },
            .pColorBlendState =
                &(VkPipelineColorBlendStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = VK_FALSE,
                    // .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = 1,
                    .pAttachments =
                        &(VkPipelineColorBlendAttachmentState){
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,
                            .blendEnable = VK_FALSE,
                        },
                },
            .pDynamicState =
                &(VkPipelineDynamicStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = dynamic_state_count,
                    .pDynamicStates = dynamic_states,
                },
        };

    vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1,
                              &trace_pipeline_create_info, NULL,
                              &renderer.trace_pipeline);

    vkDestroyShaderModule(renderer.device, trace_frag_shader, NULL);
    vkDestroyShaderModule(renderer.device, trace_vert_shader, NULL);
  }

  { // present pipeline
    const u32 binding_count = 3;
    VkDescriptorSetLayoutBinding bindings[binding_count] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
        {
            .binding = 2,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };
    VkDescriptorSetLayoutCreateInfo present_descriptor_set_layout_create_info =
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = binding_count,
            .pBindings = bindings,
        };

    ASSURE_VK(vkCreateDescriptorSetLayout(
        renderer.device, &present_descriptor_set_layout_create_info, NULL,
        &renderer.present_descriptor_set_layout));

    VkDescriptorSetAllocateInfo descriptor_set_alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = renderer.descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &renderer.present_descriptor_set_layout,
    };
    ASSURE_VK(vkAllocateDescriptorSets(renderer.device,
                                       &descriptor_set_alloc_info,
                                       &renderer.present_descriptor_set));

    VkShaderModule present_vert_shader =
        create_shader_module(renderer.device, fullscreen_quad_vert_spv_data,
                             fullscreen_quad_vert_spv_size);
    VkShaderModule present_frag_shader = create_shader_module(
        renderer.device, present_frag_spv_data, present_frag_spv_size);

    VkPipelineShaderStageCreateInfo present_shader_stage_create_infos[] = {
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = present_vert_shader,
            .pName = "main",
        },
        (VkPipelineShaderStageCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = present_frag_shader,
            .pName = "main",
        },
    };

    const u32 dynamic_state_count = 2;
    VkDynamicState dynamic_states[dynamic_state_count] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    const VkPipelineLayoutCreateInfo
        present_shader_pipeline_layout_create_info =
            (VkPipelineLayoutCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                .pushConstantRangeCount = 1,
                .pPushConstantRanges =
                    &(VkPushConstantRange){
                        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
                        .offset = 0,
                        .size = sizeof(PresentPushConstants),
                    },
                .setLayoutCount = 1,
                .pSetLayouts = &renderer.present_descriptor_set_layout,
            };

    ASSURE_VK(vkCreatePipelineLayout(
        renderer.device, &present_shader_pipeline_layout_create_info, NULL,
        &renderer.present_pipeline_layout));

    VkGraphicsPipelineCreateInfo present_pipeline_create_info =
        (VkGraphicsPipelineCreateInfo){
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = present_shader_stage_create_infos,
            .layout = renderer.present_pipeline_layout,
            .renderPass = renderer.present_render_pass,
            .subpass = 0,
            .pVertexInputState =
                &(VkPipelineVertexInputStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                    .vertexAttributeDescriptionCount = 0,
                    .pVertexAttributeDescriptions = NULL,
                },
            .pInputAssemblyState =
                &(VkPipelineInputAssemblyStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                    .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    .primitiveRestartEnable = VK_FALSE,
                },
            .pViewportState =
                &(VkPipelineViewportStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                    .viewportCount = 1,
                    .scissorCount = 1,
                },
            .pRasterizationState =
                &(VkPipelineRasterizationStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                    .polygonMode = VK_POLYGON_MODE_FILL,
                    .cullMode = VK_CULL_MODE_FRONT_BIT,
                    .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
                    .lineWidth = 1.0,
                },
            .pMultisampleState =
                &(VkPipelineMultisampleStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                    .sampleShadingEnable = VK_FALSE,
                    .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
                },
            .pDepthStencilState =
                &(VkPipelineDepthStencilStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                    .depthTestEnable = VK_TRUE,
                    .depthWriteEnable = VK_TRUE,
                    .depthCompareOp = VK_COMPARE_OP_LESS,
                    .depthBoundsTestEnable = VK_FALSE,
                    .minDepthBounds = 0.0f,
                    .maxDepthBounds = 1.0f,
                    .stencilTestEnable = VK_TRUE,
                },
            .pColorBlendState =
                &(VkPipelineColorBlendStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                    .logicOpEnable = VK_FALSE,
                    // .logicOp = VK_LOGIC_OP_COPY,
                    .attachmentCount = 1,
                    .pAttachments =
                        &(VkPipelineColorBlendAttachmentState){
                            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                              VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT |
                                              VK_COLOR_COMPONENT_A_BIT,
                            .blendEnable = VK_FALSE,
                        },
                },
            .pDynamicState =
                &(VkPipelineDynamicStateCreateInfo){
                    .sType =
                        VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                    .dynamicStateCount = dynamic_state_count,
                    .pDynamicStates = dynamic_states,
                },
        };

    vkCreateGraphicsPipelines(renderer.device, VK_NULL_HANDLE, 1,
                              &present_pipeline_create_info, NULL,
                              &renderer.present_pipeline);

    vkDestroyShaderModule(renderer.device, present_frag_shader, NULL);
    vkDestroyShaderModule(renderer.device, present_vert_shader, NULL);
  }

  create_framebuffers(&renderer);

  { // create syncronization primitives and per frame data
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      VkSemaphoreCreateInfo semaphore_create_info = (VkSemaphoreCreateInfo){
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      };
      VkSemaphore image_available;
      ASSURE_VK(vkCreateSemaphore(renderer.device, &semaphore_create_info, NULL,
                                  &image_available));
      VkSemaphore render_finished;
      ASSURE_VK(vkCreateSemaphore(renderer.device, &semaphore_create_info, NULL,
                                  &render_finished));
      VkFenceCreateInfo fence_create_info = (VkFenceCreateInfo){
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };
      VkFence in_flight;
      ASSURE_VK(
          vkCreateFence(renderer.device, &fence_create_info, NULL, &in_flight))

      renderer.frame_data[i] = (PerFrameData){
          .image_available = image_available,
          .render_finished = render_finished,
          .in_flight = in_flight,
          .command_buffer = command_buffers[i],
      };
    }
  }

  { // init camera
    mat4x4LookAt(renderer.camera_view, vec3New(0.0f, 0.0f, 1.0f),
                 vec3New(0.0f, 0.0f, 0.0f), vec3New(0.0f, 1.0f, 0.0f));

    renderer.camera_fov = 80.0;
    renderer_set_or_update_camera(&renderer);
  }

  renderer.imgui_impl = init_imgui_render_impl(&renderer, window);

  return renderer;
}

void destroy_framebuffer_attachment(Renderer *renderer,
                                    FramebufferAttachment *fba) {
  vkDestroyImageView(renderer->device, fba->view, NULL);
  vkDestroyImage(renderer->device, fba->image, NULL);
  vkFreeMemory(renderer->device, fba->memory, NULL);
}

// NOTE: this isn't actually totally compliant because the present format (for
// example) could have changed and we don't account for that. practically this
// is Fine but not actually. shrug its late and i'm sleepy lol.
void recreate_swapchain(Renderer *self) {
  vkDeviceWaitIdle(self->device);

  destroy_framebuffer_attachment(self, &self->depth_attachment);
  destroy_framebuffer_attachment(self, &self->position_attachment);
  destroy_framebuffer_attachment(self, &self->normal_attachment);
  destroy_framebuffer_attachment(self, &self->trace_output_attachment);

  for (u32 i = 0; i < self->swapchain_image_count; i++) {
    vkDestroyFramebuffer(self->device, self->swapchain_framebuffers[i], NULL);
    vkDestroyFramebuffer(self->device, self->trace_framebuffers[i], NULL);
  }

  for (u32 i = 0; i < self->swapchain_image_count; i++) {
    vkDestroyImageView(self->device, self->swapchain_image_views[i], NULL);
  }

  vkDestroySwapchainKHR(self->device, self->swapchain, NULL);
  create_swapchain(self);
  create_framebuffers(self);

  const u32 trace_input_descriptor_set_writes_count = 2;
  VkWriteDescriptorSet trace_input_descriptor_set_writes
      [trace_input_descriptor_set_writes_count] = {
          {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = self->trace_descriptor_set,
              .dstBinding = 1,
              .dstArrayElement = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
              .descriptorCount = 1,
              .pImageInfo =
                  &(VkDescriptorImageInfo){
                      .imageView = self->normal_attachment.view,
                      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .sampler = self->vec3_sampler,
                  },
          },
          {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = self->trace_descriptor_set,
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
              .descriptorCount = 1,
              .pImageInfo =
                  &(VkDescriptorImageInfo){
                      .imageView = self->position_attachment.view,
                      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .sampler = self->vec3_sampler,
                  },
          }};
  vkUpdateDescriptorSets(self->device, trace_input_descriptor_set_writes_count,
                         trace_input_descriptor_set_writes, 0, NULL);

  const u32 present_sampler_descriptor_set_writes_count = 3;
  VkWriteDescriptorSet present_sampler_descriptor_set_writes
      [present_sampler_descriptor_set_writes_count] = {
          {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = self->present_descriptor_set,
              .dstBinding = 1,
              .dstArrayElement = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .pImageInfo =
                  &(VkDescriptorImageInfo){
                      .imageView = self->normal_attachment.view,
                      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .sampler = self->vec3_sampler,
                  },
          },
          {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = self->present_descriptor_set,
              .dstBinding = 0,
              .dstArrayElement = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .pImageInfo =
                  &(VkDescriptorImageInfo){
                      .imageView = self->position_attachment.view,
                      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .sampler = self->vec3_sampler,
                  },
          },
          {
              .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet = self->present_descriptor_set,
              .dstBinding = 2,
              .dstArrayElement = 0,
              .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
              .descriptorCount = 1,
              .pImageInfo =
                  &(VkDescriptorImageInfo){
                      .imageView = self->trace_output_attachment.view,
                      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      .sampler = self->vec3_sampler,
                  },
          },
      };

  vkUpdateDescriptorSets(self->device,
                         present_sampler_descriptor_set_writes_count,
                         present_sampler_descriptor_set_writes, 0, NULL);
}

void renderer_resize(Renderer *self, u32 width, u32 height) {
  self->physical_device_info.swapchain_extent = (VkExtent2D){
      .width = width,
      .height = height,
  };

  renderer_set_or_update_camera(self);

  recreate_swapchain(self);
}

void renderer_destroy(Renderer *self) {
  vkDeviceWaitIdle(self->device);

  imgui_renderer_destroy(self, &self->imgui_impl);

  if (self->vertex_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(self->device, self->vertex_buffer, NULL);
    vkFreeMemory(self->device, self->vertex_buffer_memory, NULL);
  }

  for (usize i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
    PerFrameData *frame = &self->frame_data[i];
    vkDestroySemaphore(self->device, frame->image_available, NULL);
    vkDestroySemaphore(self->device, frame->render_finished, NULL);
    vkDestroyFence(self->device, frame->in_flight, NULL);
    vkFreeCommandBuffers(self->device, self->command_pool, 1,
                         &frame->command_buffer);
  }

  vkDestroySampler(self->device, self->vec3_sampler, NULL);

  destroy_framebuffer_attachment(self, &self->depth_attachment);
  destroy_framebuffer_attachment(self, &self->normal_attachment);
  destroy_framebuffer_attachment(self, &self->position_attachment);
  destroy_framebuffer_attachment(self, &self->trace_output_attachment);

  for (u32 i = 0; i < self->swapchain_image_count; i++) {
    vkDestroyFramebuffer(self->device, self->swapchain_framebuffers[i], NULL);
    vkDestroyFramebuffer(self->device, self->trace_framebuffers[i], NULL);
  }

  vkDestroyPipeline(self->device, self->trace_pipeline, NULL);
  vkDestroyPipelineLayout(self->device, self->trace_pipeline_layout, NULL);
  vkDestroyDescriptorSetLayout(self->device, self->trace_descriptor_set_layout,
                               NULL);

  vkDestroyPipeline(self->device, self->present_pipeline, NULL);
  vkDestroyPipelineLayout(self->device, self->present_pipeline_layout, NULL);

  vkDestroyDescriptorSetLayout(self->device,
                               self->present_descriptor_set_layout, NULL);

  vkDestroyPipeline(self->device, self->first_bounce_pipeline, NULL);
  vkDestroyPipelineLayout(self->device, self->first_bounce_pipeline_layout,
                          NULL);

  vkDestroyRenderPass(self->device, self->present_render_pass, NULL);
  vkDestroyRenderPass(self->device, self->trace_render_pass, NULL);

  vkDestroyDescriptorPool(self->device, self->descriptor_pool, NULL);

  vkDestroyCommandPool(self->device, self->command_pool, NULL);

  for (u32 i = 0; i < self->swapchain_image_count; i++) {
    vkDestroyImageView(self->device, self->swapchain_image_views[i], NULL);
  }

  vkDestroySwapchainKHR(self->device, self->swapchain, NULL);

  vkDestroyDevice(self->device, NULL);
  vkDestroySurfaceKHR(self->instance, self->surface, NULL);

  if (self->debug_messenger != VK_NULL_HANDLE) {
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            self->instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(self->instance, self->debug_messenger,
                                    NULL);
  }

  vkDestroyInstance(self->instance, NULL);
}

typedef struct {
  VkBuffer buffer;
  VkDeviceMemory memory;
} BufferResult;

BufferResult create_buffer(Renderer *self, u32 size, void *data,
                           VkBufferUsageFlags usage) {
  VkBufferCreateInfo vb_create_info = (VkBufferCreateInfo){
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .size = size,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };
  VkBuffer buffer;
  vkCreateBuffer(self->device, &vb_create_info, NULL, &buffer);

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(self->device, buffer, &mem_reqs);

  VkMemoryAllocateInfo allocate_info = (VkMemoryAllocateInfo){
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .allocationSize = mem_reqs.size,
      .memoryTypeIndex =
          find_memory_type(self, mem_reqs.memoryTypeBits,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
  };
  VkDeviceMemory memory;
  ASSURE_VK(vkAllocateMemory(self->device, &allocate_info, NULL, &memory));
  vkBindBufferMemory(self->device, buffer, memory, 0);

  void *mapped_mem;
  vkMapMemory(self->device, memory, 0, size, 0, &mapped_mem);
  memcpy(mapped_mem, data, size);
  vkUnmapMemory(self->device, memory);

  return (BufferResult){
      .buffer = buffer,
      .memory = memory,
  };
}

void renderer_set_object(Renderer *self, u32 vertex_count, Vertex *vb,
                         u32 _index_count, u32 *_ib) {
  vkDeviceWaitIdle(self->device);

  self->vertex_count = vertex_count;
  self->vb = vb;
  self->index_count = _index_count;
  self->ib = _ib;

  if (self->vertex_buffer != VK_NULL_HANDLE) {
    vkDestroyBuffer(self->device, self->vertex_buffer, NULL);
    vkFreeMemory(self->device, self->vertex_buffer_memory, NULL);
  }
  // if (self->index_buffer != VK_NULL_HANDLE)
  //   vkDestroyBuffer(self->device, self->vertex_buffer, NULL);

  BufferResult res = create_buffer(self, vertex_count * sizeof(Vertex), vb,
                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
  self->vertex_buffer = res.buffer;
  self->vertex_buffer_memory = res.memory;
}

void renderer_draw_gui(Renderer *self) {
  if (igCollapsingHeader_BoolPtr("Camera", NULL,
                                 ImGuiTreeNodeFlags_DefaultOpen |
                                     ImGuiTreeNodeFlags_CollapsingHeader)) {
    if (igSliderFloat("FOV", &self->camera_fov, 0.01, 180.0, "%.1fdeg", 0)) {
      renderer_set_or_update_camera(self);
    }
  }
}

void renderer_update(Renderer *self) {
  imgui_renderer_begin();
  PerFrameData *frame = &self->frame_data[self->frame];
  self->frame = (self->frame + 1) % MAX_FRAMES_IN_FLIGHT;

  vkWaitForFences(self->device, 1, &frame->in_flight, VK_TRUE, UINT64_MAX);

  u32 image_index;
  VkResult acquire_image_result = vkAcquireNextImageKHR(
      self->device, self->swapchain, UINT64_MAX, frame->image_available,
      VK_NULL_HANDLE, &image_index);

  if (acquire_image_result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain(self);
    return;
  }

  vkResetFences(self->device, 1, &frame->in_flight);

  VkCommandBuffer cmdbuffer = frame->command_buffer;

  vkResetCommandBuffer(cmdbuffer, 0);
  VkCommandBufferBeginInfo cmdbuffer_begin_info = (VkCommandBufferBeginInfo){
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  ASSURE_VK(vkBeginCommandBuffer(cmdbuffer, &cmdbuffer_begin_info));

  const u32 render_pass_clear_value_count = 4;
  VkClearValue render_pass_clear_values[render_pass_clear_value_count] = {
      (VkClearValue){
          .color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f},
      },
      (VkClearValue){
          .color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f},
      },
      (VkClearValue){
          .color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f},
      },
      (VkClearValue){.depthStencil = (VkClearDepthStencilValue){1.0f, 0}},
  };
  VkRenderPassBeginInfo render_pass_begin_info = (VkRenderPassBeginInfo){
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = self->trace_render_pass,
      .framebuffer = self->trace_framebuffers[image_index],
      .renderArea =
          (VkRect2D){
              .offset.x = 0,
              .offset.y = 0,
              .extent = self->physical_device_info.swapchain_extent,
          },
      .clearValueCount = render_pass_clear_value_count,
      .pClearValues = render_pass_clear_values,
  };
  vkCmdBeginRenderPass(cmdbuffer, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    self->first_bounce_pipeline);

  VkViewport viewport = {
      .x = 0,
      .y = self->physical_device_info.swapchain_extent.height,
      .width = self->physical_device_info.swapchain_extent.width,
      .height = -((f32)self->physical_device_info.swapchain_extent.height),
      .minDepth = 0.0,
      .maxDepth = 1.0,
  };

  VkRect2D scissor = {
      .offset.x = 0,
      .offset.y = 0,
      .extent = self->physical_device_info.swapchain_extent,
  };
  vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);
  vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);

  mat4x4 camera_matrix;
  mat4x4MultiplyMatrix(camera_matrix, self->camera_view,
                       self->camera_projection);

  vkCmdPushConstants(cmdbuffer, self->first_bounce_pipeline_layout,
                     VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(mat4x4),
                     &camera_matrix);

  VkBuffer buffers[] = {self->vertex_buffer};
  VkDeviceSize offsets[] = {0};
  vkCmdBindVertexBuffers(cmdbuffer, 0, 1, buffers, offsets);

  vkCmdDraw(cmdbuffer, self->vertex_count, 1, 0, 0);

  vkCmdNextSubpass(cmdbuffer, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          self->trace_pipeline_layout, 0, 1,
                          &self->trace_descriptor_set, 0, NULL);

  vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    self->trace_pipeline);
  vkCmdDraw(cmdbuffer, 3, 1, 0, 0);
  vkCmdEndRenderPass(cmdbuffer);

  const u32 present_render_pass_clear_value_count = 1;
  VkClearValue
      present_render_pass_clear_values[present_render_pass_clear_value_count] =
          {
              (VkClearValue){
                  .color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f},
              },
          };
  VkRenderPassBeginInfo present_render_pass_begin_info =
      (VkRenderPassBeginInfo){
          .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
          .renderPass = self->present_render_pass,
          .framebuffer = self->swapchain_framebuffers[image_index],
          .renderArea =
              (VkRect2D){
                  .offset.x = 0,
                  .offset.y = 0,
                  .extent = self->physical_device_info.swapchain_extent,
              },
          .clearValueCount = present_render_pass_clear_value_count,
          .pClearValues = present_render_pass_clear_values,
      };
  vkCmdBeginRenderPass(cmdbuffer, &present_render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          self->present_pipeline_layout, 0, 1,
                          &self->present_descriptor_set, 0, NULL);

  vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    self->present_pipeline);

  const u32 num_items = 3;
  const char *items[num_items] = {"position", "normal", "color"};
  static u32 current_item = 2;
  if (igBeginCombo("present mode", items[current_item], 0)) {
    for (u32 i = 0; i < num_items; i++) {
      bool selected = (current_item == i);
      if (igSelectable_Bool(items[i], selected, 0, (ImVec2){0, 0})) {
        current_item = i;
      }
      if (selected)
        igSetItemDefaultFocus();
    }
    igEndCombo();
  }

  PresentPushConstants present_push_constants = {
      .mode = current_item,
  };
  vkCmdPushConstants(cmdbuffer, self->present_pipeline_layout,
                     VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                     sizeof(PresentPushConstants), &present_push_constants);

  vkCmdDraw(cmdbuffer, 3, 1, 0, 0);

  renderer_draw_gui(self);

  imgui_renderer_end();
  imgui_renderer_update(cmdbuffer);

  vkCmdEndRenderPass(cmdbuffer);

  ASSURE_VK(vkEndCommandBuffer(cmdbuffer));

  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
  };
  VkSemaphore signal_semaphore[] = {frame->render_finished};
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &frame->image_available,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmdbuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signal_semaphore,
  };

  ASSURE_VK(
      vkQueueSubmit(self->graphics_queue, 1, &submit_info, frame->in_flight));

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signal_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &self->swapchain,
      .pImageIndices = &image_index,
      .pResults = NULL,
  };

  vkQueuePresentKHR(self->present_queue, &present_info);
  //   infoln("-------------------");
}