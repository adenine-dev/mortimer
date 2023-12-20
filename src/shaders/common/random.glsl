uvec4 rng_seed_pcg;
uvec4 rng_seed_blue;
ivec2 rng_p;

void init_random(vec2 p, uint frame) {
  rng_p = ivec2(p);

  rng_seed_pcg = uvec4(p, uint(frame), uint(p.x) + uint(p.y));
  rng_seed_blue =
      uvec4(frame, frame * 15843u, frame * 31u + 4566u, frame * 2345u + 58585u);
}

// adapted from https://www.jcgt.org/published/0009/03/02
// using 4d because its easy and also expandable if its ever needed, but really
// we only need 2
void pcg_4d(inout uvec4 v) {
  v = v * 1664525u + 1013904223u;
  v += v.yzxy * v.wxyz;
  v ^= v >> 16u;
  v += v.yzxy * v.wxyz;
}

float rand_1d() {
  pcg_4d(rng_seed_pcg);
  return float(rng_seed_pcg.x) / float(0xffffffffu);
}

vec2 rand_2d() {
  pcg_4d(rng_seed_pcg);
  return vec2(rng_seed_pcg.xy) / float(0xffffffffu);
}

// NOTE: define before including the file
#ifdef BLUE_NOISE
ivec2 shift2() {
  pcg_4d(rng_seed_blue);
  return (rng_p + ivec2(rng_seed_blue.xy % 0x0fffffffu)) % 1024;
}

vec4 rand_blue() { return texelFetch(blue_noise_tex, shift2(), 0); }

float sample_1d() { return rand_blue().x; }
vec2 sample_2d() { return rand_blue().xy; }

#else

float sample_1d() { return rand_1d(); }
vec2 sample_2d() { return rand_2d(); }

#endif