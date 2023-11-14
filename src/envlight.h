#pragma once

#include "ccVector.h"
#include "types.h"

typedef struct {
  vec4 *light_data;
  u32 width;
  u32 height;
} EnvironmentLight;

EnvironmentLight envlight_new_from_file(const char *filename);

void envlight_destroy(EnvironmentLight *envlight);