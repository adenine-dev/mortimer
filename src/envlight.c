#include "envlight.h"

#include "ccVector.h"
#include "stb_image.h"

#include "log.h"
#include "types.h"

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
