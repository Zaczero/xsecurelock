#include "config.h"

#include "../helpers/dimmer_bayer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void BuildBayerMatrix(int power, int *matrix) {
  int side = 1;
  matrix[0] = 0;

  for (int level = 1; level <= power; ++level) {
    int prev_side = side;
    side <<= 1;

    int *next = calloc((size_t)side * (size_t)side, sizeof(*next));
    assert(next != NULL);

    for (int y = 0; y < prev_side; ++y) {
      for (int x = 0; x < prev_side; ++x) {
        int value = matrix[y * prev_side + x];
        next[y * side + x] = 4 * value;
        next[y * side + (x + prev_side)] = 4 * value + 2;
        next[(y + prev_side) * side + x] = 4 * value + 3;
        next[(y + prev_side) * side + (x + prev_side)] = 4 * value + 1;
      }
    }

    memcpy(matrix, next, (size_t)side * (size_t)side * sizeof(*matrix));
    free(next);
  }
}

int main(void) {
  for (int power = 0; power <= 8; ++power) {
    int side = 1 << power;
    size_t entry_count = (size_t)side * (size_t)side;
    int *matrix = calloc(entry_count, sizeof(*matrix));
    unsigned char *seen = calloc(entry_count, sizeof(*seen));
    assert(matrix != NULL);
    assert(seen != NULL);

    BuildBayerMatrix(power, matrix);

    for (size_t index = 0; index < entry_count; ++index) {
      int x = -1;
      int y = -1;
      DimmerBayerPoint((int)index, power, &x, &y);
      assert(x >= 0 && x < side);
      assert(y >= 0 && y < side);
      assert(matrix[(size_t)y * (size_t)side + (size_t)x] == (int)index);
      assert(seen[(size_t)y * (size_t)side + (size_t)x] == 0);
      seen[(size_t)y * (size_t)side + (size_t)x] = 1;
    }

    free(seen);
    free(matrix);
  }

  return 0;
}
