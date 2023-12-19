#ifndef SHADER_COMMON_CONSTANTS
#define SHADER_COMMON_CONSTANTS

// ✧₊♥˚: *♥✧₊˚:♡ config constants ♡:˚₊✧♥* :˚♥₊✧

/// number of rays per pixel per iteration
const uint RAY_SAMPLES = 1u;

/// max bounces per ray
const uint MAX_BOUNCES = 5u;

/// number of light samples (only matters if SAMPLE_LIGHTS == true)
const uint LIGHT_SAMPLES = 1u;

/// number of samples to take when the ray escapes after no bounces (only
/// matters if there is lens radius)
const uint ESCAPED_RAY_SAMPLES = 5u;

/// enable to sample materials (UNIMPLEMENTED)
const bool SAMPLE_MATERIALS = false;

/// enable to direct sample lights
const bool SAMPLE_LIGHTS = true;

/// define to stylize the defocus blur kernal to be heart shaped
#define CFG_STYLIZE_HEART

/// define to not use the rasterized first bounce g buffers (this is needed if
/// you want defocus blur on the geometry of the scene)
// #define CFG_NO_FIRST_BOUNCE

// ✧₊♥˚: *♥✧₊˚:♡ normal constants ♡:˚₊✧♥* :˚♥₊✧

const uint NULL_OBJECT_ID = 0xffffffffu;

const float EPSILON = 1e-5;

const float PI = 3.14159265359;
const float INV_PI = 0.3183098861837907;

#endif