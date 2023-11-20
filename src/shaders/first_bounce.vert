#version 450

layout(push_constant) uniform PushConstants { mat4 camera_matrix; }
constants;

layout(location = 0) in vec4 i_position_object_index;
layout(location = 1) in vec3 i_normal;

layout(location = 0) out vec3 o_position;
layout(location = 1) out vec3 o_normal;
layout(location = 2) flat out uint o_object_index;

void main() {
  gl_Position =
      constants.camera_matrix * vec4(i_position_object_index.xyz, 1.0);
  o_position = i_position_object_index.xyz;
  o_normal = i_normal;
  o_object_index = floatBitsToUint(i_position_object_index.w);
}
