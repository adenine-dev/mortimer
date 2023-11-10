#version 450

layout(input_attachment_index = 0,
       binding = 0) uniform subpassInput sampler_frame_sample;
layout(input_attachment_index = 1,
       binding = 1) uniform subpassInput sampler_accumulation;

layout(location = 0) out vec4 col;

layout(push_constant) uniform PushConstants { uint frame; }
constants;

void main() {
  vec3 frame_sample = subpassLoad(sampler_frame_sample).xyz;
  vec3 accumulation = subpassLoad(sampler_accumulation).xyz;
  if (constants.frame == 1) {
    col = vec4(frame_sample, 1.0);
    return;
  }

  col = vec4(mix(accumulation, frame_sample, vec3(1.0 / constants.frame)), 1.0);
  //   col = vec4((frame_sample + accumulation) / 2.0, 1.0);
  //   col = vec4((accumulation), 1.0);
}