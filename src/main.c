#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL_video.h>

#include "renderer.h"
#include "types.h"

#include "log.h"

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

  SDL_Event event;
  int running = 1;

  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT) {
        running = 0;
      }

      renderer_update(&renderer);

      vkDeviceWaitIdle(renderer.device);
    }
  }

  renderer_destroy(&renderer);

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
