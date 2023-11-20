#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL_events.h"
#include "SDL_mouse.h"
#include <SDL3/SDL.h>
#include <SDL_video.h>

#include "ccVector.h"

#include "envlight.h"
#include "loader.h"
#include "log.h"
#include "maths.h"
#include "renderer.h"
#include "scene.h"
#include "trimesh.h"
#include "types.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

int main(int argc, char **argv) {
  if (SDL_Init(SDL_INIT_VIDEO) == 1) {
    errorln("Failed to init SDL");
    exit(1);
  }

  SDL_Window *window = SDL_CreateWindowWithPosition(
      "hello world", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
      SDL_WINDOW_MAXIMIZED | SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

  if (!window) {
    errorln("Failed to init Window\n");
    exit(1);
  }

  Renderer renderer = renderer_create(window);

  Scene scene = scene_new();
  scene_add_object(&scene, "assets/models/lucy.obj",
                   (Material){.albedo = vec3New(0.84, 0.9, 0.6)});
  scene_add_object(&scene, "assets/models/suzanne.obj",
                   (Material){.albedo = vec3New(0.84, 0.6, 0.9)});
  scene_add_object(&scene, "assets/models/xyzrgb_dragon.obj",
                   (Material){.albedo = vec3New(0.6, 0.84, 0.9)});
  scene_add_object(&scene, "assets/models/ground.obj",
                   (Material){.albedo = vec3New(0.2, 0.2, 0.2)});
  scene_set_envlight(&scene, "assets/hdris/sunset_jhbcentral_4k.hdr");

  renderer_set_scene(&renderer, &scene);

  SDL_Event event;
  bool running = true;

  vec3 center = vec3New(0.0, 1.0, 0.0);
  vec3 eye = vec3Add(center, vec3New(1.5, 0.0, 1.5));
  // vec3 center = vec3New(0.0, 2.0, 0.0);
  // vec3 eye = vec3Add(center, vec3New(7.0, 2.5, -7.0));
  const vec3 up = vec3New(0.0, 1.0, 0.0);
  f32 radius = vec3Length(eye);
  bool camera_updated = true;

  while (running) {
    while (SDL_PollEvent(&event)) {
      ImGui_ImplSDL3_ProcessEvent(&event);

      switch (event.type) {
      case SDL_EVENT_QUIT: {
        running = false;
      } break;
      case SDL_EVENT_WINDOW_RESIZED: {
        renderer_resize(&renderer, event.window.data1, event.window.data2);
      } break;
      case SDL_EVENT_MOUSE_WHEEL: {
        if (!igGetIO()->WantCaptureMouse) {
          radius += event.wheel.y;
          radius = clamp(radius, 0.1, 500.0);
          eye = vec3Multiply(vec3Normalize(eye), radius);
          camera_updated = true;
        }
      } break;
      case SDL_EVENT_MOUSE_MOTION: {
        if (!igGetIO()->WantCaptureMouse) {
          if (event.motion.state & SDL_BUTTON_LMASK) {
            vec2 dA =
                vec2Multiply(vec2New(-2.0 * M_PI /
                                         (f32)renderer.physical_device_info
                                             .swapchain_extent.width *
                                         event.motion.xrel,
                                     M_PI /
                                         (f32)renderer.physical_device_info
                                             .swapchain_extent.height *
                                         event.motion.yrel),
                             radius * 3.0);

            vec3 view = vec3Negate(mat4x4GetRow(renderer.camera_view, 2).xyz);
            vec3 right = mat4x4GetRow(renderer.camera_view, 0).xyz;
            vec3 up = vec3Negate(vec3CrossProduct(view, right));

            eye = vec3Add(vec3Multiply(right, dA.x), eye);
            eye = vec3Add(vec3Multiply(up, dA.y), eye);
            eye = vec3Multiply(vec3Normalize(eye), radius);
            camera_updated = true;
          }
        }
      } break;
      }

      if (camera_updated) {
        camera_updated = false;
        mat4x4LookAt(renderer.camera_view, vec3Add(eye, center), center, up);
        renderer.accumulated_frames = 0;
      }
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    igNewFrame();

    // static bool open = true;
    // igShowDemoWindow(&open);

    renderer_update(&renderer);
  }

  renderer_destroy(&renderer);

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
