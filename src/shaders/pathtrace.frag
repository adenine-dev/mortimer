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
  uint frame;
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

float ray_triangle_intersection(Ray ray, vec3 v0, vec3 v1, vec3 v2) {
  const float EPSILON = 1e-7;
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

uvec4 rng_seed;
ivec2 rng_p;
void init_random(vec2 p, uint frame) {
  rng_p = ivec2(p);
  rng_seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

float sample_1d() {
  rng_seed = rng_seed * 1664525u + 1013904223u;
  rng_seed.x += rng_seed.y * rng_seed.w;
  rng_seed.y += rng_seed.z * rng_seed.x;
  rng_seed.z += rng_seed.x * rng_seed.y;
  rng_seed.w += rng_seed.y * rng_seed.z;
  rng_seed = rng_seed ^ (rng_seed >> 16u);
  rng_seed.x += rng_seed.y * rng_seed.w;
  rng_seed.y += rng_seed.z * rng_seed.x;
  rng_seed.z += rng_seed.x * rng_seed.y;
  rng_seed.w += rng_seed.y * rng_seed.z;
  return float(rng_seed.x) / float(0xffffffffu);
}

struct SceneIntersection {
  float t;
  uint triangle_idx;
};

SceneIntersection ray_scene_intersect(Ray ray) {
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

  if (triangle_idx == 0xffffffff) {
    return SceneIntersection(-1.0, triangle_idx);
  }

  return SceneIntersection(t_max, triangle_idx);
}

vec3 escaped_ray_color(Ray ray) {
  // return vec3(1.0, 0.4, 1.0);
  return mix(vec3((ray.d.y + 1) * 0.5), vec3(1.0, 1.0, 1.0),
             vec3(1.0, 0.4, 1.0));
}

vec3 get_face_normal(uint i, vec3 p) {
  Vertex v0 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 0]];
  Vertex v1 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 1]];
  Vertex v2 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 2]];

  vec3 f0 = v0.position - p;
  vec3 f1 = v1.position - p;
  vec3 f2 = v2.position - p;

  float det =
      length(cross(v0.position - v1.position, v0.position - v2.position));
  float b0 = length(cross(f1, f2)) / det;
  float b1 = length(cross(f2, f0)) / det;
  float b2 = length(cross(f0, f1)) / det;

  return b0 * v0.normal + b1 * v1.normal + b2 * v2.normal;
}

vec3 face_forward(vec3 n, vec3 v) { return 0.0 > dot(n, v) ? -n : n; }

const float PI = 3.14159265359;

vec2 square_to_uniform_disk_concentric(vec2 u) {
  u = 2.0 * u - vec2(1.0);
  if (u.x == 0.0 && u.y == 0.0) {
    return vec2(0.0);
  }

  float r;
  float theta;
  if (abs(u.x) > abs(u.y)) {
    r = u.x;
    theta = PI / 4.0 * (u.y / u.x);
  } else {
    r = u.y;
    theta = PI / 2.0 - PI / 4.0 * (u.x / u.y);
  };

  return r * vec2(cos(theta), sin(theta));
}

vec3 square_to_cosine_hemisphere(vec2 u) {
  vec2 d = square_to_uniform_disk_concentric(u);
  float z = sqrt(max(0.0, 1.0 - d.x * d.x - d.y * d.y));
  return vec3(d, z);
}

void main() {
  vec2 uv = i_uv * vec2(1.0, -1.0) + vec2(0.0, 1.0);  // flip viewport
  init_random(gl_FragCoord.xy, constants.frame);

  vec3 normal = subpassLoad(sampler_normal).xyz;
  if (normal == vec3(0.0)) {
    col = vec4(escaped_ray_color(create_ray(uv)), 1.0);
    return;
  }

  vec3 film = vec3(0.0);
  const uint SAMPLES = 1;
  for (uint i = 0; i < SAMPLES; i++) {
    vec3 position = subpassLoad(sampler_position).xyz;

    const vec3 surface_color = vec3(0.4, 0.4, 0.8);

    const uint MAX_BOUNCES = 3;
    vec3 surface_reflectance = surface_color;
    const float EPSILON = 1e-5;

    Ray ray =
        Ray(position + normal * EPSILON,
            normalize(face_forward(
                square_to_cosine_hemisphere(vec2(sample_1d(), sample_1d())),
                normal)));
    vec3 contributed = vec3(0.0);
    uint j;
    for (j = 0; j < MAX_BOUNCES; j++) {
      SceneIntersection intersection = ray_scene_intersect(ray);
      if (intersection.triangle_idx != 0xffffffff) {
        surface_reflectance *= surface_color;
        position = (ray.o + ray.d * intersection.t);
        normal = get_face_normal(intersection.triangle_idx, position);
        ray.o = position + normal * EPSILON;
        ray.d = normalize(face_forward(
            square_to_cosine_hemisphere(vec2(sample_1d(), sample_1d())),
            normal));
      } else {
        contributed = surface_reflectance * escaped_ray_color(ray);
        break;
      }
    }

    film += contributed;
  }

  vec4 film_color = vec4(film / SAMPLES, 1.0);
  col = film_color;
}