const uint NULL_OBJECT_ID = 0xffffffffu;

const float EPSILON = 1e-5;

const float PI = 3.14159265359;
const float INV_PI = 0.3183098861837907;

// in theory these could be higher if you have a particularly powerful
// machine, prob will only ever be 1 tho.
const uint RAY_SAMPLES = 1u;
const uint MAX_BOUNCES = 3u;

// we can afford to do more than 1 sample here since
// this is fairly cheap
const uint ESCAPED_RAY_SAMPLES = 5u;
