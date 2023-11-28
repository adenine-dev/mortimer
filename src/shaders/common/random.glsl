uvec4 rng_seed;
ivec2 rng_p;

void init_random(vec2 p, uint frame) {
  rng_p = ivec2(p);
  rng_seed = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
}

float random_1d() {
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

float sample_1d() { return random_1d(); }
vec2 sample_2d() { return vec2(random_1d(), random_1d()); }