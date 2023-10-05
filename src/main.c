#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>

#include "types.h"

int main(int argc, char **argv) {
  if (SDL_Init(SDL_INIT_VIDEO) == 1) {
    printf("Failed to init SDL\n");
    exit(1);
  }

  SDL_Window *window = SDL_CreateWindowWithPosition(
      "hello world", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480,
      SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

  if (!window) {
    printf("Failed to init Window\n");
    exit(1);
  }

  SDL_Event event;
  int running = 1;

  while (running) {
    while (SDL_PollEvent(&event)) {

      if (event.type == SDL_EVENT_QUIT)
        running = 0;
    }

    SDL_GL_SwapWindow(window);
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
