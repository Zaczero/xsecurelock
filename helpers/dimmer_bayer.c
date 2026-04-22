#include "config.h"

#include "dimmer_bayer.h"

void DimmerBayerPoint(int index, int power, int *x, int *y) {
  static const int kOffsetX[4] = {0, 1, 1, 0};
  static const int kOffsetY[4] = {0, 1, 0, 1};
  int out_x = 0;
  int out_y = 0;

  for (int level = power - 1; level >= 0; --level) {
    int stride = 1 << level;
    int quadrant = index & 3;
    out_x += kOffsetX[quadrant] * stride;
    out_y += kOffsetY[quadrant] * stride;
    index >>= 2;
  }

  *x = out_x;
  *y = out_y;
}
