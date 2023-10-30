#include "imgui_renderer.h"
#include "SDL_video.h"
#include "cimgui.h"
#include "embed/FiraCode/FiraCode-Regular.h"
#include "log.h"
#include "renderer.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#define IM_UNUSED(_VAR) ((void)(_VAR))
#define IM_ASSERT(_EXPR) assert(_EXPR)
#define IM_ARRAYSIZE(_ARR) ((int)(sizeof(_ARR) / sizeof(*(_ARR))))

ImguiRendererImpl init_imgui_render_impl(Renderer *renderer,
                                         SDL_Window *window) {
  ImguiRendererImpl ret = {};
  VkDescriptorPoolSize pool_sizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
  };

  VkDescriptorPoolCreateInfo pool_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = 1000 * IM_ARRAYSIZE(pool_sizes),
      .poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes),
      .pPoolSizes = pool_sizes,
  };

  ASSURE_VK(vkCreateDescriptorPool(renderer->device, &pool_info, NULL,
                                   &ret.descriptor_pool));

  igCreateContext(NULL);

  ImGui_ImplSDL3_InitForVulkan(window);
  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = renderer->instance,
      .PhysicalDevice = renderer->physical_device_info.device,
      .Device = renderer->device,
      //   .QueueFamily = g_QueueFamily,
      .Queue = renderer->graphics_queue,
      .PipelineCache = NULL,
      .DescriptorPool = ret.descriptor_pool,
      .Subpass = 1,
      .MinImageCount = MAX_FRAMES_IN_FLIGHT,
      .ImageCount = MAX_FRAMES_IN_FLIGHT,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
      .Allocator = NULL,
      .CheckVkResultFn = NULL,
  };
  if (!ImGui_ImplVulkan_Init(&init_info, renderer->render_pass)) {
    fatalln("could not init imgui vulkan");
  }

  VkCommandBuffer command_buffer =
      renderer->frame_data[renderer->frame % 2].command_buffer;

  ASSURE_VK(vkResetCommandPool(renderer->device, renderer->command_pool, 0));

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  ASSURE_VK(vkBeginCommandBuffer(command_buffer, &begin_info));
  ImFontAtlas_AddFontFromMemoryTTF(
      igGetIO()->Fonts, (void *)FiraCode_Regular_ttf_data,
      FiraCode_Regular_ttf_size, 18.0, NULL,
      ImFontAtlas_GetGlyphRangesDefault(igGetIO()->Fonts));

  ImGui_ImplVulkan_CreateFontsTexture(command_buffer);

  VkSubmitInfo end_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
  };
  ASSURE_VK(vkEndCommandBuffer(command_buffer));
  ASSURE_VK(
      vkQueueSubmit(renderer->graphics_queue, 1, &end_info, VK_NULL_HANDLE));

  ASSURE_VK(vkDeviceWaitIdle(renderer->device));
  ImGui_ImplVulkan_DestroyFontUploadObjects();

  return ret;
}

void imgui_renderer_begin() {
  igBegin("Properties", NULL, 0);
  static bool debug_show = true;
  if (igCollapsingHeader_BoolPtr("Debug", NULL,
                                 ImGuiTreeNodeFlags_DefaultOpen)) {
    igText("Application average %.3f ms/frame (%.1f FPS)",
           1000.0f / igGetIO()->Framerate, igGetIO()->Framerate);
  }
}

void imgui_renderer_end() { igEnd(); }

void imgui_renderer_update(VkCommandBuffer cmdbuffer) {
  igRender();

  ImGui_ImplVulkan_RenderDrawData(igGetDrawData(), cmdbuffer, VK_NULL_HANDLE);
}

void imgui_renderer_destroy(Renderer *renderer, ImguiRendererImpl *impl) {
  vkDestroyDescriptorPool(renderer->device, impl->descriptor_pool, NULL);
  ImGui_ImplVulkanH_DestroyWindow(renderer->instance, renderer->device,
                                  &impl->main_window, NULL);
  ImGui_ImplVulkan_Shutdown();
}