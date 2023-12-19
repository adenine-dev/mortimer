#include "envlight.h"

#include "ccVector.h"
#include "stb_image.h"

#include "log.h"
#include "maths.h"
#include "types.h"
#include <stdlib.h>

f32 vec4_luminance(vec4 c) {
  // convert to XYZ, then use y as luminance:
  // numbers from https://en.wikipedia.org/wiki/SRGB (NOTE: this is linear rgb)
  return 0.212671f * c.v[0] + 0.715160f * c.v[1] + 0.072169f * c.v[2];
}

void envlight_init_distribution(EnvironmentLight *self) {
  f32 *row_average = malloc(sizeof(f32) * self->height);
  { // calculate luminance and integrals
    self->luminance = malloc(sizeof(f32) * self->width * self->height);
    for (u32 v = 0; v < self->height; ++v) {
      row_average[v] = 0.0;
      f32 sin_theta = sin(PI * ((f32)v + 0.5f) / (f32)self->height);
      for (u32 u = 0; u < self->width; ++u) {
        self->luminance[v * self->width + u] =
            vec4_luminance(self->light_data[v * self->width + u]) * sin_theta;

        self->image_average += self->luminance[v * self->width + u];
        row_average[v] += self->luminance[v * self->width + u];
      }
      row_average[v] /= self->width;
    }
    self->image_average /= (self->height * self->width);
  }

  f32 *marginal = malloc(sizeof(f32) * self->height);
  { // marginal
    for (u32 v = 0; v < self->height; ++v) {
      marginal[v] = row_average[v] / self->image_average;
    }
  }

  f32 *conditional = malloc(sizeof(f32) * self->width * self->height);
  { // conditional
    for (u32 v = 0; v < self->height; ++v) {
      for (u32 u = 0; u < self->width; ++u) {
        conditional[v * self->width + u] =
            self->luminance[v * self->width + u] / row_average[v];
      }
    }
  }

  { // marginal inverse
    self->marginal_inverse = malloc(sizeof(f32) * self->height);
    for (u32 v = 0; v < self->height; ++v) {
      f32 sum = 0.0;
      u32 y;
      for (y = 0; y < self->height; ++y) {
        sum += marginal[y] / self->height;
        if (sum >= ((f32)v / self->height)) {
          break;
        }
      }
      self->marginal_inverse[v] = (y + 0.5) / self->height;
    }
  }

  { // cdf conditional inverse
    self->cdf_conditional_inverse =
        malloc(sizeof(f32) * self->width * self->height);
    for (u32 v = 0; v < self->height; ++v) {
      f32 sum = 0.0;
      u32 x = 0;
      for (u32 u = 0; u < self->width; ++u) {
        for (; x < self->width; ++x) {
          sum += conditional[v * self->width + x] / self->width;
          if (sum >= ((f32)u / self->width)) {
            break;
          }
        }
        self->cdf_conditional_inverse[v * self->width + u] =
            (x + 0.5) / self->width;
      }
    }
  }

  free(row_average);
}

EnvironmentLight envlight_new_blank_sky() {
  const u32 RESOLUTION = 32;
  vec4 *light_data = malloc(sizeof(vec4) * RESOLUTION);
  for (u32 i = 0; i < RESOLUTION; ++i) {
    f32 t = (f32)i / (f32)RESOLUTION;
    light_data[i] = vec4New(1.0, 1.0 - t, 1.0, 1.0);
  }

  EnvironmentLight self = {
      .width = 1,
      .height = RESOLUTION,
      .light_data = light_data,
  };

  envlight_init_distribution(&self);

  return self;
}

EnvironmentLight envlight_new_from_file(const char *filename) {
  if (!stbi_is_hdr(filename)) {
    fatalln("cannot use non-hdr file `%s` for environment light", filename);
  }

  int width, height, channels;
  float *data = stbi_loadf(filename, &width, &height, &channels, 4);
  if (!data) {
    fatalln("could not load environment light file `%s`", filename);
  }

  vec4 *light_data = malloc(sizeof(vec4) * width * height);
  for (int i = 0; i < width * height; ++i) {
    light_data[i] = vec4New(data[i * 4 + 0], data[i * 4 + 1], data[i * 4 + 2],
                            data[i * 4 + 3]);
  }
  stbi_image_free(data);

  EnvironmentLight self = {
      .light_data = light_data,
      .width = width,
      .height = height,
  };

  envlight_init_distribution(&self);

  return self;
}

void envlight_destroy(EnvironmentLight *self) {
  free(self->light_data);
  free(self->marginal_inverse);
  free(self->cdf_conditional_inverse);
  free(self->luminance);
}
