#include <assert.h>
#include <limits.h>

#include "../rect.h"

static void AssertRect(Rect rect, int x, int y, int w, int h) {
  assert(rect.x == x);
  assert(rect.y == y);
  assert(rect.w == w);
  assert(rect.h == h);
}

static void TestContainedShrink(void) {
  Rect uncovered[4];
  size_t count =
      RectSubtract((Rect){.x = 10, .y = 20, .w = 100, .h = 60},
                   (Rect){.x = 30, .y = 35, .w = 40, .h = 15}, uncovered);

  assert(count == 4);
  AssertRect(uncovered[0], 10, 20, 100, 15);
  AssertRect(uncovered[1], 10, 50, 100, 30);
  AssertRect(uncovered[2], 10, 35, 20, 15);
  AssertRect(uncovered[3], 70, 35, 40, 15);
}

static void TestShiftedOverlap(void) {
  Rect uncovered[4];
  size_t count =
      RectSubtract((Rect){.x = 10, .y = 10, .w = 50, .h = 40},
                   (Rect){.x = 20, .y = 15, .w = 50, .h = 40}, uncovered);

  assert(count == 2);
  AssertRect(uncovered[0], 10, 10, 50, 5);
  AssertRect(uncovered[1], 10, 15, 10, 35);
}

static void TestDisjoint(void) {
  Rect uncovered[4];
  size_t count =
      RectSubtract((Rect){.x = 0, .y = 0, .w = 30, .h = 20},
                   (Rect){.x = 50, .y = 50, .w = 10, .h = 10}, uncovered);

  assert(count == 1);
  AssertRect(uncovered[0], 0, 0, 30, 20);
}

static void TestClipNearIntBoundaries(void) {
  Rect clipped;
  assert(RectClip((Rect){.x = INT_MAX - 10, .y = INT_MAX - 20, .w = 10, .h = 20},
                  (Rect){.x = INT_MAX - 5, .y = INT_MAX - 6, .w = 5, .h = 6},
                  &clipped));
  AssertRect(clipped, INT_MAX - 5, INT_MAX - 6, 5, 6);

  assert(!RectClip((Rect){.x = INT_MAX, .y = 0, .w = 1, .h = 1},
                   (Rect){.x = 0, .y = 0, .w = 10, .h = 10}, &clipped));
}

static void TestContainsPointNearIntBoundaries(void) {
  Rect rect = {.x = INT_MAX - 2, .y = 0, .w = 2, .h = 1};
  assert(RectContainsPoint(rect, INT_MAX - 1, 0));
  assert(!RectContainsPoint(rect, INT_MAX, 0));
}

static void TestSubtractNearIntBoundaries(void) {
  Rect uncovered[4];
  size_t count =
      RectSubtract((Rect){.x = INT_MAX - 10, .y = 0, .w = 10, .h = 10},
                   (Rect){.x = INT_MIN, .y = 0, .w = 1, .h = 1}, uncovered);

  assert(count == 1);
  AssertRect(uncovered[0], INT_MAX - 10, 0, 10, 10);
}

int main(void) {
  TestContainedShrink();
  TestShiftedOverlap();
  TestDisjoint();
  TestClipNearIntBoundaries();
  TestContainsPointNearIntBoundaries();
  TestSubtractNearIntBoundaries();
  return 0;
}
