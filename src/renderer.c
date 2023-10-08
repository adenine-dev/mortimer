#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <vulkan/vulkan_core.h>

#include "SDL_video.h"
#include "log.h"
#include "maths.h"
#include "renderer.h"
#include "shaders/triangle_frag_spv.h"
#include "shaders/triangle_vert_spv.h"
#include "types.h"

#define ASSURE_VK(expr)                                                        \
  {                                                                            \
    VkResult assure_vk_result = expr;                                          \
    if (assure_vk_result != VK_SUCCESS) {                                      \
      errorln("Vulkan expr `%s` failed with code %d\n", #expr,                 \
              assure_vk_result);                                               \
      exit(1);                                                                 \
    }                                                                          \
  }

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

static const u32 REQUIRED_DEVICE_EXTENSION_COUNT = 1;
static const char *REQUIRED_DEVICE_EXTENSIONS[REQUIRED_DEVICE_EXTENSION_COUNT] =
    {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

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
    if (device_info.surface_capabilities.currentExtent.width != UINT32_MAX) {
      device_info.swapchain_extent =
          device_info.surface_capabilities.currentExtent;
    } else {
      int width, height;
      SDL_GetWindowSizeInPixels(window, &width, &height);
      device_info.swapchain_extent = (VkExtent2D){
          .width = clamp((u32)width,
                         device_info.surface_capabilities.minImageExtent.width,
                         device_info.surface_capabilities.maxImageExtent.width),
          .height =
              clamp((u32)height,
                    device_info.surface_capabilities.minImageExtent.height,
                    device_info.surface_capabilities.maxImageExtent.height),
      };
    }

    free(present_modes);
    free(formats);
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

Renderer renderer_create(SDL_Window *window) {
  // create instance
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

  bool enable_validation_layers = true;
  const u32 validation_layer_count = 1;
  const char *validation_layers[validation_layer_count] = {
      "VK_LAYER_KHRONOS_validation",
  };
  if (!assure_validation_layer_support(1, validation_layers)) {
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
              .apiVersion = VK_API_VERSION_1_0,
          },
  };

  if (enable_validation_layers) {
    create_info.enabledLayerCount = validation_layer_count;
    create_info.ppEnabledLayerNames = validation_layers;
  }

  VkInstance instance;
  ASSURE_VK(vkCreateInstance(&create_info, NULL, &instance));

  // create debug messenger
  VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
  if (enable_validation_layers) {
    PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT != NULL) {
      ASSURE_VK(vkCreateDebugUtilsMessengerEXT(
          instance, &debug_utils_create_info, NULL, &debug_messenger));
    } else {
      warnln("could not create debug util messenger");
    }
  }

  // create surface
  VkSurfaceKHR surface;
  SDL_Vulkan_CreateSurface(window, instance, &surface);

  // create physical device
  PhysicalDeviceInfo physical_device_info =
      get_physical_device(instance, window, surface);

  // create logical device
  f32 queue_priority = 1.0;
  const u32 queue_create_info_count =
      physical_device_info.graphics_family_index ==
              physical_device_info.present_family_index
          ? 1
          : 2;
  VkDeviceQueueCreateInfo queue_create_infos[queue_create_info_count];
  if (physical_device_info.graphics_family_index ==
      physical_device_info.present_family_index) {
    queue_create_infos[0] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = physical_device_info.graphics_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
  } else {
    queue_create_infos[0] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = physical_device_info.graphics_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    queue_create_infos[1] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = physical_device_info.present_family_index,
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
    device_create_info.enabledLayerCount = validation_layer_count;
    device_create_info.ppEnabledLayerNames = validation_layers;
  }

  VkDevice device;
  ASSURE_VK(vkCreateDevice(physical_device_info.device, &device_create_info,
                           NULL, &device));

  VkQueue graphics_queue;
  vkGetDeviceQueue(device, physical_device_info.graphics_family_index, 0,
                   &graphics_queue);

  VkQueue present_queue;
  vkGetDeviceQueue(device, physical_device_info.present_family_index, 0,
                   &present_queue);

  free(extensions);
  free(device_extensions);

  // create swapchain
  u32 swapchain_image_count =
      physical_device_info.surface_capabilities.minImageCount + 1;
  if (physical_device_info.surface_capabilities.maxImageCount != 0 &&
      swapchain_image_count >
          physical_device_info.surface_capabilities.maxImageCount) {
    swapchain_image_count =
        physical_device_info.surface_capabilities.maxImageCount;
  }
  VkSwapchainCreateInfoKHR swapchain_create_info = (VkSwapchainCreateInfoKHR){
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = swapchain_image_count,
      .imageFormat = physical_device_info.surface_format.format,
      .imageColorSpace = physical_device_info.surface_format.colorSpace,
      .imageExtent = physical_device_info.swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform =
          physical_device_info.surface_capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = physical_device_info.present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  u32 queue_family_indicies[2] = {
      physical_device_info.graphics_family_index,
      physical_device_info.present_family_index,
  };
  if (physical_device_info.graphics_family_index !=
      physical_device_info.present_family_index) {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount = 2;

    swapchain_create_info.pQueueFamilyIndices = queue_family_indicies;
  }

  VkSwapchainKHR swapchain;
  ASSURE_VK(
      vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain));

  vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL);
  VkImage *swapchain_images = malloc(sizeof(VkImage) * swapchain_image_count);
  vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count,
                          swapchain_images);

  VkImageView *swapchain_image_views =
      malloc(sizeof(VkImageView) * swapchain_image_count);

  VkImageViewCreateInfo swapchain_image_view_create_info =
      (VkImageViewCreateInfo){
          .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
          .viewType = VK_IMAGE_VIEW_TYPE_2D,
          .format = physical_device_info.surface_format.format,
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
  for (u32 i = 0; i < swapchain_image_count; i++) {
    swapchain_image_view_create_info.image = swapchain_images[i];

    ASSURE_VK(vkCreateImageView(device, &swapchain_image_view_create_info, NULL,
                                &swapchain_image_views[i]));
  }

  VkCommandPoolCreateInfo command_pool_create_info = (VkCommandPoolCreateInfo){
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .queueFamilyIndex = physical_device_info.graphics_family_index,
      .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
  };

  VkCommandPool command_pool;
  ASSURE_VK(vkCreateCommandPool(device, &command_pool_create_info, NULL,
                                &command_pool));
  VkCommandBufferAllocateInfo command_buffer_alloc_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool,
      .commandBufferCount = 1,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  };

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(device, &command_buffer_alloc_info, &command_buffer);

  VkAttachmentDescription color_attachment_desc = (VkAttachmentDescription){
      .format = physical_device_info.surface_format.format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };

  VkAttachmentReference color_attachment_ref = (VkAttachmentReference){
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkSubpassDescription subpass_desc = (VkSubpassDescription){
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = 1,
      .pColorAttachments = &color_attachment_ref,
  };

  VkSubpassDependency render_pass_dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .srcAccessMask = 0,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
  };

  VkRenderPassCreateInfo render_pass_create_info = (VkRenderPassCreateInfo){
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_attachment_desc,
      .subpassCount = 1,
      .pSubpasses = &subpass_desc,
      .dependencyCount = 1,
      .pDependencies = &render_pass_dependency,
  };
  VkRenderPass render_pass;
  ASSURE_VK(
      vkCreateRenderPass(device, &render_pass_create_info, NULL, &render_pass))

  VkShaderModule triangle_vert_shader = create_shader_module(
      device, triangle_vert_spv_data, triangle_vert_spv_size);
  VkShaderModule triangle_frag_shader = create_shader_module(
      device, triangle_frag_spv_data, triangle_frag_spv_size);

  VkPipelineShaderStageCreateInfo triangle_shader_stage_create_infos[] = {
      (VkPipelineShaderStageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_VERTEX_BIT,
          .module = triangle_vert_shader,
          .pName = "main",
      },
      (VkPipelineShaderStageCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
          .module = triangle_frag_shader,
          .pName = "main",
      },
  };

  const u32 dynamic_state_count = 2;
  VkDynamicState dynamic_states[dynamic_state_count] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
  };

  VkPipelineLayoutCreateInfo triangle_shader_pipeline_layout_create_info =
      (VkPipelineLayoutCreateInfo){
          .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      };
  VkPipelineLayout triangle_shader_pipeline_layout;
  ASSURE_VK(vkCreatePipelineLayout(device,
                                   &triangle_shader_pipeline_layout_create_info,
                                   NULL, &triangle_shader_pipeline_layout));

  VkGraphicsPipelineCreateInfo triangle_pipeline_create_info =
      (VkGraphicsPipelineCreateInfo){
          .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
          .stageCount = 2,
          .pStages = triangle_shader_stage_create_infos,
          .layout = triangle_shader_pipeline_layout,
          .renderPass = render_pass,
          .subpass = 0,
          .pVertexInputState =
              &(VkPipelineVertexInputStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                  .vertexBindingDescriptionCount = 0,
                  .vertexAttributeDescriptionCount = 0,
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
                  .frontFace = VK_FRONT_FACE_CLOCKWISE,
                  .lineWidth = 1.0,
              },
          .pMultisampleState =
              &(VkPipelineMultisampleStateCreateInfo){
                  .sType =
                      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                  .sampleShadingEnable = VK_FALSE,
                  .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
              },
          // .pDepthStencilState = NULL,
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
                  .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                  .dynamicStateCount = dynamic_state_count,
                  .pDynamicStates = dynamic_states,
              },
      };

  VkPipeline triangle_pipeline;
  vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1,
                            &triangle_pipeline_create_info, NULL,
                            &triangle_pipeline);

  vkDestroyShaderModule(device, triangle_frag_shader, NULL);
  vkDestroyShaderModule(device, triangle_vert_shader, NULL);

  VkFramebuffer *swapchain_framebuffers =
      malloc(swapchain_image_count * sizeof(VkFramebuffer));
  for (u32 i = 0; i < swapchain_image_count; i++) {
    VkImageView attachments[] = {swapchain_image_views[i]};

    VkFramebufferCreateInfo framebuffer_create_info = (VkFramebufferCreateInfo){
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass,
        .attachmentCount = 1,
        .pAttachments = attachments,
        .width = physical_device_info.swapchain_extent.width,
        .height = physical_device_info.swapchain_extent.height,
        .layers = 1,
    };

    ASSURE_VK(vkCreateFramebuffer(device, &framebuffer_create_info, NULL,
                                  &swapchain_framebuffers[i]));
  }

  VkSemaphoreCreateInfo semaphore_create_info = (VkSemaphoreCreateInfo){
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VkSemaphore image_available;
  ASSURE_VK(vkCreateSemaphore(device, &semaphore_create_info, NULL,
                              &image_available));
  VkSemaphore render_finished;
  ASSURE_VK(vkCreateSemaphore(device, &semaphore_create_info, NULL,
                              &render_finished));
  VkFenceCreateInfo fence_create_info = (VkFenceCreateInfo){
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VkFence in_flight;
  ASSURE_VK(vkCreateFence(device, &fence_create_info, NULL, &in_flight))

  return (Renderer){
      .instance = instance,
      .debug_messenger = debug_messenger,
      .physical_device_info = physical_device_info,
      .device = device,
      .graphics_queue = graphics_queue,
      .present_queue = present_queue,
      .surface = surface,
      .swapchain = swapchain,
      .swapchain_image_count = swapchain_image_count,
      .swapchain_images = swapchain_images,
      .swapchain_image_views = swapchain_image_views,

      .command_pool = command_pool,
      .command_buffer = command_buffer,

      .render_pass = render_pass,
      .triangle_shader_pipeline_layout = triangle_shader_pipeline_layout,
      .triangle_pipeline = triangle_pipeline,

      .swapchain_framebuffers = swapchain_framebuffers,

      .image_available = image_available,
      .render_finished = render_finished,
      .in_flight = in_flight,
  };
}

void renderer_destroy(Renderer *renderer) {
  vkDestroySemaphore(renderer->device, renderer->image_available, NULL);
  vkDestroySemaphore(renderer->device, renderer->render_finished, NULL);
  vkDestroyFence(renderer->device, renderer->in_flight, NULL);

  for (u32 i = 0; i < renderer->swapchain_image_count; i++) {
    vkDestroyFramebuffer(renderer->device, renderer->swapchain_framebuffers[i],
                         NULL);
  }

  vkDestroyPipeline(renderer->device, renderer->triangle_pipeline, NULL);
  vkDestroyPipelineLayout(renderer->device,
                          renderer->triangle_shader_pipeline_layout, NULL);
  vkDestroyRenderPass(renderer->device, renderer->render_pass, NULL);

  vkFreeCommandBuffers(renderer->device, renderer->command_pool, 1,
                       &renderer->command_buffer);
  vkDestroyCommandPool(renderer->device, renderer->command_pool, NULL);

  for (u32 i = 0; i < renderer->swapchain_image_count; i++) {
    vkDestroyImageView(renderer->device, renderer->swapchain_image_views[i],
                       NULL);
  }

  vkDestroySwapchainKHR(renderer->device, renderer->swapchain, NULL);
  vkDestroyDevice(renderer->device, NULL);
  vkDestroySurfaceKHR(renderer->instance, renderer->surface, NULL);

  if (renderer->debug_messenger != VK_NULL_HANDLE) {
    PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            renderer->instance, "vkDestroyDebugUtilsMessengerEXT");

    vkDestroyDebugUtilsMessengerEXT(renderer->instance,
                                    renderer->debug_messenger, NULL);
  }

  vkDestroyInstance(renderer->instance, NULL);
}

void renderer_update(Renderer *self) {
  vkWaitForFences(self->device, 1, &self->in_flight, VK_TRUE, UINT64_MAX);
  vkResetFences(self->device, 1, &self->in_flight);

  u32 image_index;
  vkAcquireNextImageKHR(self->device, self->swapchain, UINT64_MAX,
                        self->image_available, VK_NULL_HANDLE, &image_index);

  VkCommandBuffer cmdbuffer = self->command_buffer;
  vkResetCommandBuffer(cmdbuffer, 0);
  VkCommandBufferBeginInfo cmdbuffer_begin_info = (VkCommandBufferBeginInfo){
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  };
  ASSURE_VK(vkBeginCommandBuffer(cmdbuffer, &cmdbuffer_begin_info));
  VkRenderPassBeginInfo render_pass_begin_info = (VkRenderPassBeginInfo){
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = self->render_pass,
      .framebuffer = self->swapchain_framebuffers[image_index],
      .renderArea =
          (VkRect2D){
              .offset.x = 0,
              .offset.y = 0,
              .extent = self->physical_device_info.swapchain_extent,
          },
      .clearValueCount = 1,
      .pClearValues =
          &(VkClearValue){
              .color = (VkClearColorValue){0.0f, 0.0f, 0.0f, 1.0f},
          },
  };
  vkCmdBeginRenderPass(cmdbuffer, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                    self->triangle_pipeline);

  VkViewport viewport = {
      .x = 0,
      .y = 0,
      .width = self->physical_device_info.swapchain_extent.width,
      .height = self->physical_device_info.swapchain_extent.height,
      .minDepth = 0.0,
      .maxDepth = 1.0,
  };
  vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);

  VkRect2D scissor = {
      .offset.x = 0,
      .offset.y = 0,
      .extent = self->physical_device_info.swapchain_extent,
  };
  vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);
  vkCmdDraw(cmdbuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(cmdbuffer);

  ASSURE_VK(vkEndCommandBuffer(cmdbuffer));

  VkPipelineStageFlags wait_stages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signal_semaphore[] = {self->render_finished};
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &self->image_available,
      .pWaitDstStageMask = wait_stages,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmdbuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signal_semaphore,
  };

  ASSURE_VK(
      vkQueueSubmit(self->graphics_queue, 1, &submit_info, self->in_flight));

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
}