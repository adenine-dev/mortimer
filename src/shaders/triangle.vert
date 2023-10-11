#version 450

layout( push_constant ) uniform PushConstants
{
	mat4 camera_matrix;
} constants;

vec3 positions[3] = vec3[](
    vec3(0.0, -0.5, 0.0),
    vec3(0.5, 0.5, 0.0),
    vec3(-0.5, 0.5, 0.0)
);

void main() {
    gl_Position = constants.camera_matrix * vec4(positions[gl_VertexIndex], 1.0);
}
