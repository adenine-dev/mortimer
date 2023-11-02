#version 450

layout (binding = 0) uniform sampler2D sampler_position;
layout (binding = 1) uniform sampler2D sampler_normal;
layout (binding = 2) uniform sampler2D sampler_color;

layout( location = 0 ) in vec2 i_uv;

layout( location = 0 ) out vec4 col;

layout( push_constant ) uniform PushConstants {
    uint present_mode; // 0 for position, 1 for normal, 2 for color
} constants;

void main() {
    vec3 color = vec3(1.0, 0.0, 1.0);
    switch (constants.present_mode) {
        case 0: {
            color = texture(sampler_position, i_uv).rgb;
        } break;
        case 1: {
            color = (texture(sampler_normal, i_uv).rgb + 1.0) * 0.5;
            if (texture(sampler_normal, i_uv).rgb == vec3(0.0)) {
                color = vec3(0.0);
            }
        } break;
        case 2: {
            color = texture(sampler_color, i_uv).rgb;
        } break;
    }

    col = vec4(color, 1.0);
}