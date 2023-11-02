#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput sampler_position;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput sampler_normal;

layout( location = 0 ) in vec2 i_uv;

layout( location = 0 ) out vec4 col;

void main() {
    const vec3 light_position = vec3(1.0);
    vec3 position = subpassLoad(sampler_position).rgb;
    vec3 normal = subpassLoad(sampler_normal).rgb;
    if (normal != vec3(0.0)) {
        col = vec4(max(vec3(0.0), dot(normal, normalize(light_position - position)) * vec3(1.0)) + vec3(0.01), 1.0);
    } else {
        col = vec4(0.0, 0.0, 0.0, 1.0);
    }
}