#include "config.h"

#include "rect.h"

static int RectRight(Rect rect) { return rect.x + rect.w; }

static int RectBottom(Rect rect) { return rect.y + rect.h; }

static int RectIsEmpty(Rect rect) { return rect.w <= 0 || rect.h <= 0; }

size_t RectSubtract(Rect old_rect, Rect new_rect, Rect out[4]) {
  if (RectIsEmpty(old_rect)) {
    return 0;
  }

  Rect intersection = {
      .x = old_rect.x > new_rect.x ? old_rect.x : new_rect.x,
      .y = old_rect.y > new_rect.y ? old_rect.y : new_rect.y,
      .w = (RectRight(old_rect) < RectRight(new_rect) ? RectRight(old_rect)
                                                      : RectRight(new_rect)),
      .h = (RectBottom(old_rect) < RectBottom(new_rect)
                ? RectBottom(old_rect)
                : RectBottom(new_rect)),
  };
  intersection.w -= intersection.x;
  intersection.h -= intersection.y;

  if (RectIsEmpty(intersection)) {
    out[0] = old_rect;
    return 1;
  }

  size_t count = 0;

  if (intersection.y > old_rect.y) {
    out[count++] = (Rect){
        .x = old_rect.x,
        .y = old_rect.y,
        .w = old_rect.w,
        .h = intersection.y - old_rect.y,
    };
  }

  if (RectBottom(intersection) < RectBottom(old_rect)) {
    out[count++] = (Rect){
        .x = old_rect.x,
        .y = RectBottom(intersection),
        .w = old_rect.w,
        .h = RectBottom(old_rect) - RectBottom(intersection),
    };
  }

  if (intersection.x > old_rect.x) {
    out[count++] = (Rect){
        .x = old_rect.x,
        .y = intersection.y,
        .w = intersection.x - old_rect.x,
        .h = intersection.h,
    };
  }

  if (RectRight(intersection) < RectRight(old_rect)) {
    out[count++] = (Rect){
        .x = RectRight(intersection),
        .y = intersection.y,
        .w = RectRight(old_rect) - RectRight(intersection),
        .h = intersection.h,
    };
  }

  return count;
}
