#version 450
#extension GL_GOOGLE_include_directive : require

#include "./common/constants.glsl"
#include "./common/random.glsl"

layout(location = 0) in vec2 i_uv;

layout(location = 0) out vec4 col;

layout(input_attachment_index = 0,
       binding = 0) uniform subpassInput sampler_position;
layout(input_attachment_index = 1,
       binding = 1) uniform subpassInput sampler_normal;
layout(input_attachment_index = 2,
       binding = 2) uniform usubpassInput sampler_object_id;

struct Vertex {
  vec4 position_object_id;
  vec3 normal;
  uint _pad1;
};

layout(std140, set = 0, binding = 3) readonly buffer VertexBuffer {
  Vertex vertices[];
}
vertex_buffer;

layout(set = 0, binding = 4) readonly buffer IndexBuffer { uint indices[]; }
index_buffer;

// `l` and `r` are the `w` component of `min_l` and `max_r` respectively, for
// alignment reasons
struct BvhNode {
  vec4 min_l;
  vec4 max_r;
};

layout(set = 0, binding = 5) readonly buffer BvhBuffer { BvhNode nodes[]; }
bvh;

layout(set = 0, binding = 6) uniform sampler2D environment_map;
layout(set = 0, binding = 7) uniform sampler2D environment_map_inv_cdf;
layout(set = 0, binding = 8) uniform sampler2D environment_map_inv_marginal;

struct Material {
  vec3 albedo;
  uint _pad0;
};

layout(set = 0, binding = 9) readonly buffer MaterialsBuffer {
  Material materials[];
}
material_buffer;

layout(push_constant) uniform PushConstants {
  mat4 view_matrix;
  mat4 projection_matrix;
  uint vertex_count;
  uint index_count;
  uint node_count;
  uint object_count;
  uint frame;
  float env_focal_dist;
  float env_lens_radius;
  float environment_map_pdf_scale;
}
constants;

struct Ray {
  vec3 o;
  vec3 d;
};

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

vec2 square_to_disk(vec2 u) {
  float r = sqrt(u.x);
  float theta = 2.0 * PI * u.y;

  return r * vec2(sin(theta), cos(theta));
}

vec2 square_to_heartish(vec2 u) {
  // return u;
  vec2 ret = square_to_disk(u);
  ret.x = (ret.x + sqrt(abs(sin(ret.y)))) * 0.5;
  return ret;
}

struct Frame {
  vec3 n;
  vec3 s;
  vec3 t;
};

Frame new_frame(vec3 n) {
  float sign = n.z < 0.0 ? -1.0 : 1.0;
  float a = 1.0 / -(sign + n.z);
  float b = n.x * n.y * a;

  return Frame(
      n, normalize(vec3(1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x)),
      normalize(vec3(b, sign + n.y * n.y * a, -n.y)));
}

vec3 frame_to_local(Frame frame, vec3 v) {
  return vec3(dot(v, frame.s), dot(v, frame.t), dot(v, frame.n));
}

vec3 frame_to_world(Frame frame, vec3 v) {
  return frame.n * v.z + frame.t * v.y + frame.s * v.x;
}

Ray create_ray(vec2 uv, vec2 sample2) {
  uv = (uv * 2.0 - 1.0);
  vec4 ndsh = vec4(uv, -1.0, 1.0);
  vec4 view = vec4((inverse(constants.projection_matrix) * ndsh).xyz, 0.0);
  vec3 dir = normalize((inverse(constants.view_matrix) * view).xyz);

  Ray ray = Ray(vec3(0.0), dir);

  if (constants.env_lens_radius > 0.0) {
#ifdef CFG_STYLIZE_HEART
    // needs a more complicated, slower method to construct the onb to prevent
    // weird discontinuities
    float theta = PI / 2.0;
    mat4 rotator_z = mat4(cos(theta), -sin(theta), 0, 0,  // format newline
                          sin(theta), cos(theta), 0, 0,   //
                          0, 0, 1, 0,                     //
                          0, 0, 0, 1);

    vec4 ndsh_perp =
        vec4(1.0, uv.y, -uv.x, 1.0);  // ndsh * 90deg around the y axis
    vec4 view_perp =
        vec4((inverse(constants.projection_matrix) * ndsh_perp).xyz, 0.0);
    vec3 s =
        normalize((inverse(rotator_z * constants.view_matrix) * view_perp).xyz);

    vec3 t = cross(s, dir);
    Frame frame = Frame(dir, s, t);
#else
    Frame frame = new_frame(dir);
#endif

    vec3 focus = ray.o + ray.d * (constants.env_focal_dist / length(ray.d));

#ifdef CFG_STYLIZE_HEART
    vec2 offset = square_to_heartish(sample2);
#else
    vec2 offset = square_to_uniform_disk_concentric(sample2);
#endif

    ray.o =
        (frame.s * offset.x + frame.t * offset.y) * constants.env_lens_radius;
    ray.d = -normalize(ray.o - focus);
  }

  ray.o += -constants.view_matrix[3].xyz * mat3(constants.view_matrix);

  return ray;
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

struct SceneIntersection {
  float t;
  uint object_id;
  uint triangle_idx;
};

SceneIntersection ray_scene_intersect(Ray ray) {
  const uint TO_VISIT_LEN = 64;
  uint to_visit[TO_VISIT_LEN];
  uint node_idx = constants.node_count - 1;
  uint to_visit_idx = 0;

  float t_max = 100000000.0;
  uint triangle_idx;
  uint object_id = NULL_OBJECT_ID;

  uint intersection_count = 0;
  while (true) {
    BvhNode node = bvh.nodes[node_idx];
    intersection_count++;
    if (ray_aabb_intersection(ray, node.min_l.xyz, node.max_r.xyz)) {
      intersection_count++;
      if (floatBitsToUint(node.min_l.w) ==
          floatBitsToUint(node.max_r.w)) {  // leaf node
        uint i = floatBitsToUint(node.min_l.w);
        vec3 v0 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 0]]
                      .position_object_id.xyz;
        vec3 v1 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 1]]
                      .position_object_id.xyz;
        vec3 v2 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 2]]
                      .position_object_id.xyz;
        float t = ray_triangle_intersection(ray, v0, v1, v2);
        if (0.0 < t && t < t_max) {
          triangle_idx = i;
          object_id = floatBitsToUint(
              vertex_buffer.vertices[index_buffer.indices[i * 3 + 0]]
                  .position_object_id.w);
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

  if (object_id == NULL_OBJECT_ID) {
    return SceneIntersection(-1.0, NULL_OBJECT_ID, triangle_idx);
  }

  return SceneIntersection(t_max, object_id, triangle_idx);
}

bool ray_scene_unoccluded(Ray ray) {
  const uint TO_VISIT_LEN = 64;
  uint to_visit[TO_VISIT_LEN];
  uint node_idx = constants.node_count - 1;
  uint to_visit_idx = 0;

  uint intersection_count = 0;
  while (true) {
    BvhNode node = bvh.nodes[node_idx];
    intersection_count++;
    if (ray_aabb_intersection(ray, node.min_l.xyz, node.max_r.xyz)) {
      intersection_count++;
      if (floatBitsToUint(node.min_l.w) ==
          floatBitsToUint(node.max_r.w)) {  // leaf node
        uint i = floatBitsToUint(node.min_l.w);
        vec3 v0 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 0]]
                      .position_object_id.xyz;
        vec3 v1 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 1]]
                      .position_object_id.xyz;
        vec3 v2 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 2]]
                      .position_object_id.xyz;
        float t = ray_triangle_intersection(ray, v0, v1, v2);
        if (0.0 < t) {
          return false;
        }
      } else {
        node_idx = floatBitsToUint(node.min_l.w);
        to_visit[++to_visit_idx] = floatBitsToUint(node.max_r.w);
        continue;
      }
    }

    if (to_visit_idx == 0) return true;
    node_idx = to_visit[to_visit_idx--];
  }

  return true;
}

vec3 get_face_normal(uint i, vec3 p) {
  Vertex v0 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 0]];
  Vertex v1 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 1]];
  Vertex v2 = vertex_buffer.vertices[index_buffer.indices[i * 3 + 2]];

  vec3 p0 = v0.position_object_id.xyz;
  vec3 p1 = v1.position_object_id.xyz;
  vec3 p2 = v2.position_object_id.xyz;

  vec3 f0 = p0 - p;
  vec3 f1 = p1 - p;
  vec3 f2 = p2 - p;

  float det = length(cross(p0 - p1, p0 - p2));
  float b0 = length(cross(f1, f2)) / det;
  float b1 = length(cross(f2, f0)) / det;
  float b2 = length(cross(f0, f1)) / det;

  return b0 * v0.normal + b1 * v1.normal + b2 * v2.normal;
}

vec3 face_forward(vec3 n, vec3 v) { return 0.0 > dot(n, v) ? -n : n; }

vec3 square_to_cosine_hemisphere(vec2 u) {
  vec2 d = square_to_uniform_disk_concentric(u);
  float z = sqrt(max(0.0, 1.0 - d.x * d.x - d.y * d.y));
  return vec3(d, z);
}

vec3 square_to_uniform_hemisphere(vec2 u) {
  float z = u[0];
  float r = sqrt(max(0.0, 1.0 - z * z));
  float phi = 2 * PI * u[1];
  return vec3(r * cos(phi), r * sin(phi), z);
}

struct SufraceInteraction {
  vec3 position;
  vec3 normal;
  vec3 wo_world;
};

Ray spawn_ray(SufraceInteraction si, vec3 dir) {
  return Ray(si.position + si.normal * EPSILON,
             normalize(face_forward(dir, si.normal)));
}

SufraceInteraction get_surface_interaction(Ray ray,
                                           SceneIntersection intersection) {
  vec3 position = ray.o + ray.d * intersection.t;
  return SufraceInteraction(
      position, get_face_normal(intersection.triangle_idx, position), -ray.d);
}

vec3 eval_material(Material material, SufraceInteraction si, vec3 wi_world) {
  // Frame frame = new_frame(si.normal);
  // vec3 wo = frame_to_local(frame, si.wo_world);
  // vec3 wi = frame_to_local(frame, wi_world);

  return material.albedo * INV_PI;
}

vec3 escaped_ray_color(Ray ray) {
  vec2 uv = vec2(0.5 + (atan(ray.d.z, ray.d.x) / (PI * 2)),
                 0.5 - asin(ray.d.y) * INV_PI);
  return texture(environment_map, uv).xyz;
}

struct LightSample {
  vec3 color;
  vec3 wi;
  float pdf;
};

float luminance(vec3 c) {
  return 0.212671 * c.r + 0.715160 * c.g + 0.072169 * c.b;
}

LightSample sample_light(SufraceInteraction si) {
  vec2 random = sample_2d();
  float v = texture(environment_map_inv_marginal, vec2(0.5, random.y)).r;
  float u = texture(environment_map_inv_cdf, vec2(random.x, v)).r;
  float theta = PI * (v);
  float phi = 2.0 * PI * (0.5 + u);
  vec3 d = vec3(sin(theta) * cos(phi), cos(theta), sin(theta) * sin(phi));

  vec3 c = escaped_ray_color(Ray(vec3(0.0), d));
  if (sin(theta) == 0.0) {
    return LightSample(vec3(0.0), vec3(0.0), 0.0);
  }

  return LightSample(c, d,
                     (luminance(c) / constants.environment_map_pdf_scale) /
                         (2.0 * PI * sin(theta)));
}

vec3 direct_light_sample(SufraceInteraction si, Material material) {
  vec3 contributed = vec3(0.0);

  for (uint i = 0; i < LIGHT_SAMPLES; ++i) {
    LightSample light_sample = sample_light(si);
    Ray r = spawn_ray(si, light_sample.wi);
    if (ray_scene_unoccluded(r)) {
      float cos_theta = dot(light_sample.wi, si.normal);
      if (cos_theta > 0.0 && light_sample.pdf > 0.0) {
        vec3 f = eval_material(material, si, light_sample.wi) * cos_theta;
        contributed += f * escaped_ray_color(Ray(vec3(0.0), light_sample.wi)) /
                       (light_sample.pdf);
      }
    }
  }

  return contributed / LIGHT_SAMPLES;
}

void main() {
  vec2 uv = i_uv * vec2(1.0, -1.0) + vec2(0.0, 1.0);  // flip viewport
  init_random(gl_FragCoord.xy, constants.frame);

#ifndef CFG_NO_FIRST_BOUNCE
  if (subpassLoad(sampler_object_id).x == NULL_OBJECT_ID) {
    col = vec4(0.0);
    for (uint i = 0; i < ESCAPED_RAY_SAMPLES; i++) {
      col += vec4(escaped_ray_color(create_ray(uv, sample_2d())), 1.0);
    }
    // col = vec4(create_ray(uv, sample_2d()).d, 1.0);
    col /= ESCAPED_RAY_SAMPLES;
    return;
  }
#endif

  const vec3 camera_eye =
      -constants.view_matrix[3].xyz * mat3(constants.view_matrix);

  vec3 film = vec3(0.0);

  for (uint i = 0; i < RAY_SAMPLES; i++) {
    vec3 contributed = vec3(0.0);
    vec3 surface_reflectance = vec3(1.0);

#ifdef CFG_NO_FIRST_BOUNCE
    uint object_id;
    vec3 position;
    vec3 normal;
    Material material;
    Ray ray = create_ray(uv, sample_2d());
#else
    uint object_id = subpassLoad(sampler_object_id).x;
    vec3 normal = subpassLoad(sampler_normal).xyz;
    vec3 position = subpassLoad(sampler_position).xyz;
    Material material = material_buffer.materials[object_id];

    SufraceInteraction first_bounce_interaction =
        SufraceInteraction(position, normal, -normalize(camera_eye - position));

    if (SAMPLE_LIGHTS) {
      contributed += surface_reflectance *
                     direct_light_sample(first_bounce_interaction, material);
    }

    const vec3 wi = square_to_uniform_hemisphere(sample_2d());
    Frame frame = new_frame(first_bounce_interaction.normal);
    vec3 wi_world = frame_to_world(frame, wi);
    const float pdf = (INV_PI * 0.5);
    surface_reflectance *=
        eval_material(material, first_bounce_interaction, wi_world) *
        abs(dot(wi_world, first_bounce_interaction.normal)) / pdf;

    Ray ray = spawn_ray(first_bounce_interaction, wi_world);
#endif

    for (uint j = 0; j < MAX_BOUNCES - 1; j++) {
      SceneIntersection intersection = ray_scene_intersect(ray);
      if (intersection.object_id != NULL_OBJECT_ID) {
        SufraceInteraction si = get_surface_interaction(ray, intersection);
        position = si.position;
        normal = si.normal;
        object_id = intersection.object_id;
        material = material_buffer.materials[object_id];

        if (SAMPLE_LIGHTS) {
          contributed +=
              surface_reflectance * direct_light_sample(si, material);
        }

        if (SAMPLE_MATERIALS) {
          // UNIMPLEMENTED
        } else {
          vec3 wi = square_to_uniform_hemisphere(sample_2d());
          Frame frame = new_frame(si.normal);
          vec3 wi_world = frame_to_world(frame, wi);
          const float pdf = (INV_PI * 0.5);
          surface_reflectance *= eval_material(material, si, wi_world) *
                                 abs(dot(wi_world, si.normal)) / pdf;

          ray = spawn_ray(si, wi_world);
        }
      } else {
        // ray has escaped
        contributed += surface_reflectance * escaped_ray_color(ray);

        break;
      }
    }

    film += contributed;
  }

  vec4 film_color = vec4(film / RAY_SAMPLES, 1.0);
  col = film_color;
}