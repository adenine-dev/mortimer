#include "envlight.h"

#include "ccVector.h"
#include "stb_image.h"

#include "log.h"
#include "types.h"

EnvironmentLight envlight_new_blank_sky() {
  const u32 RESOLUTION = 32;
  vec4 *light_data = malloc(sizeof(vec4) * RESOLUTION);
  for (u32 i = 0; i < RESOLUTION; ++i) {
    f32 t = (f32)i / (f32)RESOLUTION;
    light_data[i] = vec4New(1.0, 1.0 - t, 1.0, 1.0);
  }

  return (EnvironmentLight){
      .width = 1,
      .height = RESOLUTION,
      .light_data = light_data,
  };
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

  return (EnvironmentLight){
      .light_data = light_data,
      .width = width,
      .height = height,
  };
}

void envlight_destroy(EnvironmentLight *envlight) {
  free(envlight->light_data);
}
