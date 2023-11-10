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

ImVec4 hex(u32 hex) {
  f32 r, g, b, a = 1.0;
  if (hex >= 0x1000000) {
    r = (((hex) >> 24) & 0xFF) / 255.0;
    g = (((hex) >> 16) & 0xFF) / 255.0;
    b = (((hex) >> 8) & 0xFF) / 255.0;
    a = ((hex)&0xFF) / 255.0;
  } else {
    r = (((hex) >> 16) & 0xFF) / 255.0;
    g = (((hex) >> 8) & 0xFF) / 255.0;
    b = (((hex)) & 0xFF) / 255.0;
  }

  return (ImVec4){r, g, b, a};
}

ImVec4 hexa(u32 hex, u32 a) {
  f32 r = (((hex) >> 16) & 0xFF) / 255.0;
  f32 g = (((hex) >> 8) & 0xFF) / 255.0;
  f32 b = (((hex)) & 0xFF) / 255.0;

  return (ImVec4){r, g, b, a / 255.0};
}

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
      .Queue = renderer->graphics_queue,
      .PipelineCache = NULL,
      .DescriptorPool = ret.descriptor_pool,
      .Subpass = 0,
      .MinImageCount = MAX_FRAMES_IN_FLIGHT,
      .ImageCount = MAX_FRAMES_IN_FLIGHT,
      .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
      .Allocator = NULL,
      .CheckVkResultFn = NULL,
  };
  if (!ImGui_ImplVulkan_Init(&init_info, renderer->present_render_pass)) {
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
      FiraCode_Regular_ttf_size, 18.5, NULL,
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

  // set styles
  ImGuiStyle *style = igGetStyle();
  const u32 background = 0x1F212A;
  const u32 bg_dark = 0x232830;
  const u32 frame = 0x343d46;
  const u32 pink = 0xdda2f6;
  const u32 grab = 0x65737e;

  style->Alpha = 1.0;
  style->FrameRounding = 4.0;
  style->WindowRounding = 4.0;
  style->FrameRounding = 2.0;
  style->PopupRounding = 2.0;
  style->ChildRounding = 4.0;
  style->ScrollbarRounding = 10000.0;
  style->TabRounding = 3.0;

  style->SeparatorTextBorderSize = 1.0;
  style->Colors[ImGuiCol_Text] = hex(0xc0c5ceff);
  style->Colors[ImGuiCol_TextDisabled] = hex(0x555C77ff);

  style->Colors[ImGuiCol_WindowBg] = hex(background);
  style->Colors[ImGuiCol_ChildBg] = hex(bg_dark);
  style->Colors[ImGuiCol_PopupBg] = hex(background);

  style->Colors[ImGuiCol_Border] = hexa(grab, 0xaa);
  style->Colors[ImGuiCol_BorderShadow] = hex(bg_dark);

  style->Colors[ImGuiCol_FrameBg] = hex(0x343d4677);
  style->Colors[ImGuiCol_FrameBgHovered] = hex(0x343d46cc);
  style->Colors[ImGuiCol_FrameBgActive] = hex(0x343d46ff);

  style->Colors[ImGuiCol_TitleBg] = hexa(bg_dark, 0xff);
  style->Colors[ImGuiCol_TitleBgCollapsed] = hexa(bg_dark, 0xaa);
  style->Colors[ImGuiCol_TitleBgActive] = hexa(bg_dark, 0xff);

  style->Colors[ImGuiCol_MenuBarBg] = hexa(bg_dark, 0xff);

  style->Colors[ImGuiCol_ScrollbarBg] = hexa(bg_dark, 0xff);
  style->Colors[ImGuiCol_ScrollbarGrab] = hexa(pink, 0x99);
  style->Colors[ImGuiCol_ScrollbarGrabHovered] = hexa(pink, 0xaa);
  style->Colors[ImGuiCol_ScrollbarGrabActive] = hexa(pink, 0xcc);

  style->Colors[ImGuiCol_CheckMark] = hex(pink);

  style->Colors[ImGuiCol_SliderGrab] = hexa(pink, 0xaa);
  style->Colors[ImGuiCol_SliderGrabActive] = hexa(pink, 0xdd);

  style->Colors[ImGuiCol_Button] = hexa(pink, 0x33);
  style->Colors[ImGuiCol_ButtonHovered] = hexa(pink, 0x44);
  style->Colors[ImGuiCol_ButtonActive] = hexa(pink, 0x99);

  style->Colors[ImGuiCol_Header] = hex(0x343d4622);
  style->Colors[ImGuiCol_HeaderHovered] = hex(0x343d46cc);
  style->Colors[ImGuiCol_HeaderActive] = hex(0x343d46ff);

  style->Colors[ImGuiCol_Separator] = hexa(pink, 0x99);
  style->Colors[ImGuiCol_SeparatorHovered] = hexa(pink, 0xbb);
  style->Colors[ImGuiCol_SeparatorActive] = hex(pink);

  style->Colors[ImGuiCol_ResizeGrip] = hexa(grab, 0xaa);
  style->Colors[ImGuiCol_ResizeGripHovered] = hexa(grab, 0xdd);
  style->Colors[ImGuiCol_ResizeGripActive] = hexa(grab, 0xff);

  style->Colors[ImGuiCol_Tab] = hex(frame);
  style->Colors[ImGuiCol_TabHovered] = hexa(pink, 0xaa);
  style->Colors[ImGuiCol_TabActive] = hexa(pink, 0x99);
  style->Colors[ImGuiCol_TabUnfocused] = hexa(frame, 0xaa);
  style->Colors[ImGuiCol_TabUnfocusedActive] = hexa(pink, 0x77);

  style->Colors[ImGuiCol_DockingPreview] = hex(pink);
  style->Colors[ImGuiCol_DockingEmptyBg] = hex(bg_dark);

  style->Colors[ImGuiCol_PlotLines] = hexa(pink, 0xcc);
  style->Colors[ImGuiCol_PlotLinesHovered] = hex(pink);
  style->Colors[ImGuiCol_PlotHistogram] = hexa(pink, 0xcc);
  style->Colors[ImGuiCol_PlotHistogramHovered] = hex(pink);

  style->Colors[ImGuiCol_TableHeaderBg] = hex(bg_dark);
  style->Colors[ImGuiCol_TableBorderStrong] = hexa(pink, 0xdd);
  style->Colors[ImGuiCol_TableBorderLight] = hexa(pink, 0xaa);
  style->Colors[ImGuiCol_TableRowBg] = hex(background);
  style->Colors[ImGuiCol_TableRowBgAlt] = hex(bg_dark);

  style->Colors[ImGuiCol_TextSelectedBg] = hexa(pink, 0x44);

  style->Colors[ImGuiCol_DragDropTarget] = hex(pink);

  // style->Colors[ImGuiCol_NavHighlight] = hex(unset);
  // style->Colors[ImGuiCol_NavWindowingHighlight] = hex(unset);
  // style->Colors[ImGuiCol_NavWindowingDimBg] = hex(unset);
  // style->Colors[ImGuiCol_ModalWindowDimBg] = hex(unset);

  return ret;
}

void imgui_renderer_begin(Renderer *renderer) {
  igBegin("Properties", NULL, 0);
  static bool debug_show = true;
  if (igCollapsingHeader_BoolPtr("Debug", NULL,
                                 ImGuiTreeNodeFlags_DefaultOpen)) {
    igText("Application average %.3f ms/frame (%.1f FPS)",
           1000.0f / igGetIO()->Framerate, igGetIO()->Framerate);
    igText("Accumulated frames: %u", renderer->accumulated_frames);
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