#pragma once

#define clamp(_clamp_val, _clamp_min, _clamp_max)                              \
  (((_clamp_val) < (_clamp_min))                                               \
       ? (_clamp_min)                                                          \
       : ((_clamp_val) > (_clamp_max) ? (_clamp_max) : (_clamp_val)))