#pragma once

#include "ccVector.h"
#include "math.h"
#include "types.h"

#define clamp(_clamp_val, _clamp_min, _clamp_max)                              \
  (((_clamp_val) < (_clamp_min))                                               \
       ? (_clamp_min)                                                          \
       : ((_clamp_val) > (_clamp_max) ? (_clamp_max) : (_clamp_val)))

#define sgn(_sgn_val)                                                          \
  (((_sgn_val) > 0.0) ? 1.0 : ((_sgn_val) < 0.0) ? -1.0 : 0.0)

#define min(_min_val_0, _min_val_1)                                            \
  (((_min_val_0) < (_min_val_1))                                               \
       ? (_min_val_0)                                                          \
       : (_min_val_1)

#define max(_max_val_0, _max_val_1)                                            \
  (((_max_val_0) < (_max_val_1))                                               \
       ? (_max_val_0)                                                          \
       : (_max_val_1)

// NOTE: using the same style as ccvector for consistency
static inline vec3 vec3Min(vec3 a, vec3 b) {
  return vec3New(fminf(a.x, b.x), fminf(a.y, b.y), fminf(a.z, b.z));
}

static inline vec3 vec3Max(vec3 a, vec3 b) {
  return vec3New(fmaxf(a.x, b.x), fmaxf(a.y, b.y), fmaxf(a.z, b.z));
}

typedef struct {
  vec3 min;
  vec3 max;
} Aabb;

static inline Aabb aabb_new(vec3 min, vec3 max) {
  return (Aabb){
      .min = vec3Min(min, max),
      .max = vec3Max(max, min),
  };
}

static inline Aabb aabb_from_vec3(vec3 p) {
  return (Aabb){
      .min = vec3Subtract(p, vec3New(1e-6, 1e-6, 1e-6)),
      .max = vec3Add(p, vec3New(1e-6, 1e-6, 1e-6)),
  };
}

static inline u32 aabb_max_extent_idx(Aabb self) {
  vec3 d = vec3Subtract(self.max, self.min);
  return (d.x > d.y && d.x > d.z) ? 0 : ((d.y > d.z) ? 1 : 2);
}

static inline Aabb aabb_union(Aabb self, Aabb other) {
  return (Aabb){
      .min = vec3Min(self.min, other.min),
      .max = vec3Max(self.max, other.max),
  };
}

static inline Aabb aabb_expand(Aabb self, vec3 p) {
  return (Aabb){
      .min = vec3Min(self.min, p),
      .max = vec3Max(self.max, p),
  };
}

static inline vec3 aabb_centroid(Aabb self) {
  return vec3Multiply(vec3Add(self.min, self.max), 0.5f);
}

static inline vec3 aabb_offset(Aabb self, vec3 p) {
  vec3 o = vec3Subtract(p, self.min);
  for (u8 i = 0; i < 3; i++) {
    if (self.max.v[i] > self.min.v[i]) {
      o.v[i] /= self.max.v[i] - self.min.v[i];
    }
  }

  return o;
}

static inline f32 aabb_surface_area(Aabb self) {
  vec3 d = vec3Subtract(self.max, self.min);

  return 2.0f * (d.x * d.y + d.x * d.z + d.y * d.z);
}