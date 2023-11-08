#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "SDL_events.h"
#include "SDL_mouse.h"
#include <SDL3/SDL.h>
#include <SDL_video.h>

#include "ccVector.h"

#include "loader.h"
#include "log.h"
#include "maths.h"
#include "renderer.h"
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

  ObjMesh obj = load_obj("assets/suzanne.obj");
  // ObjMesh obj = load_obj("assets/xyzrgb_dragon.obj");
  Vertex *vb = malloc(obj.vertex_count * sizeof(Vertex));
  for (usize i = 0; i < obj.vertex_count; i++) {
    vb[i] = (Vertex){
        .position = obj.positions[i],
        .normal = obj.normals[i],
    };
  }
  TriangleMesh mesh =
      trimesh_new(vb, obj.vertex_count, obj.indicies, obj.index_count);
  renderer_set_object(&renderer, &mesh);
  // renderer_set_object(&renderer, obj.vertex_count, vb, obj.index_count,
  //                     obj.indicies);

  SDL_Event event;
  bool running = true;

  vec3 center = vec3Zero();
  vec3 eye = vec3New(0.0, 0.0, 2.0);
  const vec3 up = vec3New(0.0, 1.0, 0.0);
  f32 radius = 2.0;

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
          }
        }
      } break;
      }

      mat4x4LookAt(renderer.camera_view, vec3Add(eye, center), center, up);
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();

    igNewFrame();

    // static bool open = true;
    // igShowDemoWindow(&open);

    renderer_update(&renderer);
  }

  renderer_destroy(&renderer);
  trimesh_destroy(mesh);

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
