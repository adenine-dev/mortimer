#version 450

// layout (binding = 0) uniform sampler2D sampler_position;
// layout (binding = 1) uniform sampler2D sampler_normal;

layout (input_attachment_index = 0, binding = 0) uniform subpassInput sampler_position;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput sampler_normal;

layout( location = 0 ) in vec2 i_uv;

layout( location = 0 ) out vec4 col;

layout( push_constant ) uniform PushConstants {
	uint mode; // 0 for position, 1 for normal
} constants;

void main() {
    vec3 color = vec3(1.0, 0.0, 1.0);
    switch (constants.mode) {
        case 0: {
            color = subpassLoad(sampler_position).rgb;
        } break;
        case 1: {
            color = (subpassLoad(sampler_normal).rgb + 1.0) * 0.5;
            if (subpassLoad(sampler_normal).rgb == vec3(0.0)) {
                color = vec3(0.0);
            }
        } break;
    }

    col = vec4(color, 1.0);
}