#version 450

layout(location = 0) in vec3 i_position;
layout(location = 1) in vec3 i_normal;
layout(location = 2) flat in uint i_object_index;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;
layout(location = 2) out uint object_index;

void main() {
  position = vec4(i_position, 1.0);
  normal = vec4(i_normal.xyz, 1.0);
  object_index = i_object_index;
  // object_index = gl_PrimitiveID;
}