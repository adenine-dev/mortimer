#pragma once

#include "ccVector.h"
#include "types.h"

typedef struct {
  vec4 *light_data;
  u32 width;
  u32 height;

  f32 *luminance;
  f32 image_average;

  f32 *cdf_conditional_inverse;
  f32 *marginal_inverse;

} EnvironmentLight;

EnvironmentLight envlight_new_from_file(const char *filename);
EnvironmentLight envlight_new_blank_sky();

void envlight_destroy(EnvironmentLight *envlight);