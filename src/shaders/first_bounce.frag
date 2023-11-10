#version 450

layout(location = 0) in vec3 i_normal;
layout(location = 1) in vec3 i_position;

layout(location = 0) out vec4 position;
layout(location = 1) out vec4 normal;

void main() {
  position = vec4(i_position, 1.0);
  normal = vec4(i_normal, 1.0);
}