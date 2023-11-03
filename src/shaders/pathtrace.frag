#version 450

layout (input_attachment_index = 0, binding = 0) uniform subpassInput sampler_position;
layout (input_attachment_index = 1, binding = 1) uniform subpassInput sampler_normal;

layout( location = 0 ) in vec2 i_uv;

layout( location = 0 ) out vec4 col;

struct Vertex {
    vec3 position;
    uint _pad0;
    vec3 normal;
    uint _pad1;
};

layout(std140, set = 0, binding = 2) readonly buffer VertexBuffer {
	Vertex vertices[];
} vertex_buffer;

layout(set = 0, binding = 3) readonly buffer IndexBuffer {
	uint indices[];
} index_buffer;

layout( push_constant ) uniform PushConstants {
    mat4 view_matrix;
    mat4 projection_matrix;
    uint vertex_count;
    uint index_count;
} constants;

struct Ray {
    vec3 o;
    vec3 d;
};

Ray create_ray(vec2 uv) {
    vec3 eye = -constants.view_matrix[3].xyz * mat3(constants.view_matrix);
    vec4 ndsh = vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 view = vec4((inverse(constants.projection_matrix) * ndsh).xyz, 0.0);
    vec3 view_dir = normalize((inverse(constants.view_matrix) * view).xyz);

    return Ray(eye, view_dir);
}

const float EPSILON = 1e-6;
float ray_triangle_intersection(Ray ray, vec3 v0, vec3 v1, vec3 v2) {    
    // Möller–Trumbore intersection
    vec3 edge1 = v1 - v0;
    vec3 edge2 = v2 - v0;
    vec3 h = cross(ray.d, edge2);
    float a = dot(edge1, h);

    if (-EPSILON < a && a < EPSILON) {
        return -1.0;
    }

    float f = 1.0 / a;
    vec3 s = ray.o - v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return -1.0;

    vec3 q = cross(s, edge1);
    float v = f * dot(ray.d, q);

    if (v < 0.0 || u + v > 1.0)
        return -1.0;

    float t = f * dot(edge2, q);
    if (t < EPSILON)
        return -1.0;
    
    return t;
}

void main() {
    vec2 uv = i_uv * vec2(1.0, -1.0) + vec2(0.0, 1.0); // flip viewport
    Ray ray = create_ray(uv);

    for (uint i = 0; i < constants.index_count; i += 3) {
        vec3 v0 = vertex_buffer.vertices[index_buffer.indices[i + 0]].position;
        vec3 v1 = vertex_buffer.vertices[index_buffer.indices[i + 1]].position;
        vec3 v2 = vertex_buffer.vertices[index_buffer.indices[i + 2]].position;

        if (ray_triangle_intersection(ray, v0, v1, v2) > 0.0) {
            col = vec4(0.9, 0.6, 1.0, 1.0);
            break;
        }
    }
}