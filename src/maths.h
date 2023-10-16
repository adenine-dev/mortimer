#pragma once

#define clamp(_clamp_val, _clamp_min, _clamp_max)                              \
  (((_clamp_val) < (_clamp_min))                                               \
       ? (_clamp_min)                                                          \
       : ((_clamp_val) > (_clamp_max) ? (_clamp_max) : (_clamp_val)))

#define sgn(_sgn_val)                                                          \
  (((_sgn_val) > 0.0) ? 1.0 : ((_sgn_val) < 0.0) ? -1.0 : 0.0)