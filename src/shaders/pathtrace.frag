#version 450

layout(input_attachment_index = 0,
       binding = 0) uniform subpassInput sampler_position;
layout(input_attachment_index = 1,
       binding = 1) uniform subpassInput sampler_normal;

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 col;

struct Vertex {
  vec3 position;
  uint _pad0;
  vec3 normal;
  uint _pad1;
};

layout(std140, set = 0, binding = 2) readonly buffer VertexBuffer {
  Vertex vertices[];
}
vertex_buffer;

layout(set = 0, binding = 3) readonly buffer IndexBuffer { uint indices[]; }
index_buffer;

// `l` and `r` are the `w` component of `min_l` and `max_r` respectively, for
// alignment reasons
struct BvhNode {
  vec4 min_l;
  vec4 max_r;
};

layout(set = 0, binding = 4) readonly buffer BvhBuffer { BvhNode nodes[]; }
bvh;

layout(push_constant) uniform PushConstants {
  mat4 view_matrix;
  mat4 projection_matrix;
  uint vertex_count;
  uint index_count;
  uint node_count;
}
constants;

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

  if (u < 0.0 || u > 1.0) return -1.0;

  vec3 q = cross(s, edge1);
  float v = f * dot(ray.d, q);

  if (v < 0.0 || u + v > 1.0) {
    return -1.0;
  }

  float t = f * dot(edge2, q);
  if (t < EPSILON) {
    return -1.0;
  }

  return t;
}

bool ray_aabb_intersection(Ray ray, vec3 aabb_min, vec3 aabb_max) {
  vec3 t_min = (aabb_min - ray.o) / ray.d;
  vec3 t_max = (aabb_max - ray.o) / ray.d;
  vec3 t0 = min(t_min, t_max);
  vec3 t1 = max(t_min, t_max);
  float near = max(max(t0.x, t0.y), t0.z);
  float far = min(min(t1.x, t1.y), t1.z);
  return near < far;
}

void main() {
  vec2 uv = i_uv * vec2(1.0, -1.0) + vec2(0.0, 1.0);  // flip viewport
  Ray ray = create_ray(uv);

  const uint TO_VISIT_LEN = 32;
  uint to_visit[TO_VISIT_LEN];
  uint node_idx = constants.node_count - 1;
  uint to_visit_idx = 0;

  float t_max = 100000000.0;
  uint triangle_idx = 0xffffffff;

  uint intersection_count = 0;
  while (true) {
    BvhNode node = bvh.nodes[node_idx];
    intersection_count++;
    if (ray_aabb_intersection(ray, node.min_l.xyz, node.max_r.xyz)) {
      intersection_count++;
      if (floatBitsToUint(node.min_l.w) ==
          floatBitsToUint(node.max_r.w)) {  // leaf node
        uint i = floatBitsToUint(node.min_l.w);
        vec3 v0 =
            vertex_buffer.vertices[index_buffer.indices[i * 3 + 0]].position;
        vec3 v1 =
            vertex_buffer.vertices[index_buffer.indices[i * 3 + 1]].position;
        vec3 v2 =
            vertex_buffer.vertices[index_buffer.indices[i * 3 + 2]].position;
        float t = ray_triangle_intersection(ray, v0, v1, v2);
        if (0.0 < t && t < t_max) {
          triangle_idx = i;
          t_max = t;
        }
      } else {
        node_idx = floatBitsToUint(node.min_l.w);
        to_visit[++to_visit_idx] = floatBitsToUint(node.max_r.w);
        continue;
      }
    }

    if (to_visit_idx == 0) break;
    node_idx = to_visit[to_visit_idx--];
  }

  vec3 colors[12] =
      vec3[12](vec3(1.0, 0.0, 0.0), vec3(1.0, 1.0, 0.0), vec3(0.0, 1.0, 0.0),
               vec3(0.0, 1.0, 1.0), vec3(0.0, 0.0, 1.0), vec3(1.0, 0.0, 1.0),

               vec3(1.0, 0.5, 0.5), vec3(1.0, 1.0, 0.5), vec3(0.5, 1.0, 0.5),
               vec3(0.5, 1.0, 1.0), vec3(0.5, 0.5, 1.0), vec3(1.0, 0.5, 1.0));
  if (triangle_idx != 0xffffffff) {
    // col = vec4(0.9, 0.6, 1.0, 1.0);
    col = vec4(colors[triangle_idx % 12], 1.0);
  } else if (subpassLoad(sampler_normal).xyz != vec3(0.0)) {
    col = vec4(0.01, 0.01, 0.01, 1.0);
  }

  // col = vec4(vec3(float(intersection_count), 0.0, 0.0), 1.0);
}