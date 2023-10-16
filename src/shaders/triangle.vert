#version 450

layout( push_constant ) uniform PushConstants {
	mat4 camera_matrix;
} constants;

layout( location = 0 ) in vec3 i_position;
layout( location = 1 ) in vec3 i_normal;

layout( location = 0) out vec3 o_normal;

void main() {
    vec3 position = i_position.xyz;
    gl_Position = constants.camera_matrix * vec4(position, 1.0);
    o_normal = i_normal;
}
