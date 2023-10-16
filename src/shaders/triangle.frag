#version 450

layout( location = 0) in vec3 i_normal;

layout( location = 0 ) out vec4 col;

void main() {
    col = vec4((i_normal + 1.0) / 2.0, 1.0);
}