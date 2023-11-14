#version 450

layout(binding = 0) uniform sampler2D sampler_position;
layout(binding = 1) uniform sampler2D sampler_normal;
layout(binding = 2) uniform sampler2D sampler_color;
layout(binding = 3) uniform sampler2D sampler_accumulation;

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 col;

layout(push_constant) uniform PushConstants {
  uint present_mode;  // 0 for position, 1 for normal, 2 for color, 3 for
                      // accumulation
}
constants;

const uint PRESENT_MODE_POSITION = 0;
const uint PRESENT_MODE_NORMAL = 1;
const uint PRESENT_MODE_COLOR = 2;
const uint PRESENT_MODE_ACCUMULATION = 3;

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve
vec3 tonemap_aces(vec3 x) {
  float a = 2.51;
  float b = 0.03;
  float c = 2.43;
  float d = 0.59;
  float e = 0.14;
  return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main() {
  vec3 color = vec3(1.0, 0.0, 1.0);
  switch (constants.present_mode) {
    case PRESENT_MODE_POSITION: {
      color = texture(sampler_position, i_uv).rgb;
    } break;
    case PRESENT_MODE_NORMAL: {
      color = (texture(sampler_normal, i_uv).rgb + 1.0) * 0.5;
      if (texture(sampler_normal, i_uv).rgb == vec3(0.0)) {
        color = vec3(0.0);
      }
    } break;
    case PRESENT_MODE_COLOR: {
      color = texture(sampler_color, i_uv).rgb;
    } break;
    case PRESENT_MODE_ACCUMULATION: {
      color = texture(sampler_accumulation, i_uv).rgb;
    } break;
  }

  if (constants.present_mode == PRESENT_MODE_COLOR ||
      constants.present_mode == PRESENT_MODE_ACCUMULATION) {
    color = tonemap_aces(color);
  } else {
    // gamma correct
    color = max(vec3(0.0), color);
    color = pow(color, vec3(1.0 / 2.2));
  }

  col = vec4(color, 1.0);
}