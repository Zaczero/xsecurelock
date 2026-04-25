#include "config.h"

#include "../helpers/dimmer_opacity.h"

#include <assert.h>
#include <stdint.h>

int main(void) {
  assert(DimmerOpacityFromSrgbAlpha(-1.0) == 0);
  assert(DimmerOpacityFromSrgbAlpha(0.0) == 0);
  assert(DimmerOpacityFromSrgbAlpha(0.5) == UINT32_C(2147483647));
  assert(DimmerOpacityFromSrgbAlpha(1.0) == UINT32_MAX);
  assert(DimmerOpacityFromSrgbAlpha(2.0) == UINT32_MAX);

  return 0;
}
