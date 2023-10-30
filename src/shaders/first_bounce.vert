#version 450

layout( push_constant ) uniform PushConstants {
	mat4 camera_matrix;
} constants;

layout( location = 0 ) in vec3 i_position;
layout( location = 1 ) in vec3 i_normal;

layout( location = 0 ) out vec3 o_normal;
layout( location = 1 ) out vec3 o_position;

void main() {
    gl_Position = constants.camera_matrix * vec4(i_position, 1.0);
    o_normal = i_normal;
    o_position = i_position;
}
