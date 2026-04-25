#ifndef XSECURELOCK_HELPERS_DIMMER_OPACITY_H
#define XSECURELOCK_HELPERS_DIMMER_OPACITY_H

#include <math.h>
#include <stdint.h>

static inline uint32_t DimmerOpacityFromSrgbAlpha(double srgb_alpha) {
  double raw = nextafter(4294967296.0, 0.0) * srgb_alpha;
  if (!(raw > 0.0)) {
    return 0;
  }
  if (raw >= 4294967295.0) {
    return UINT32_MAX;
  }
  return (uint32_t)raw;
}

#endif
