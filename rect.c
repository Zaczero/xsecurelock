#include "config.h"

#include "rect.h"

#include <limits.h>
#include <stdint.h>

static int Int64ToInt(int64_t value, int *out) {
  if (value < INT_MIN || value > INT_MAX) {
    return 0;
  }
  *out = (int)value;
  return 1;
}

static int RectEndChecked(int start, int size, int *end) {
  if (size < 0) {
    return 0;
  }
  return Int64ToInt((int64_t)start + (int64_t)size, end);
}

static int RectIsEmpty(Rect rect) { return rect.w <= 0 || rect.h <= 0; }

static int64_t RectRight(Rect rect) { return (int64_t)rect.x + rect.w; }

static int64_t RectBottom(Rect rect) { return (int64_t)rect.y + rect.h; }

int RectClip(Rect rect, Rect clip, Rect *out) {
  int rect_right;
  int rect_bottom;
  int clip_right;
  int clip_bottom;
  if (RectIsEmpty(rect) || RectIsEmpty(clip) ||
      !RectEndChecked(rect.x, rect.w, &rect_right) ||
      !RectEndChecked(rect.y, rect.h, &rect_bottom) ||
      !RectEndChecked(clip.x, clip.w, &clip_right) ||
      !RectEndChecked(clip.y, clip.h, &clip_bottom)) {
    return 0;
  }

  int left = rect.x > clip.x ? rect.x : clip.x;
  int top = rect.y > clip.y ? rect.y : clip.y;
  int right = rect_right < clip_right ? rect_right : clip_right;
  int bottom = rect_bottom < clip_bottom ? rect_bottom : clip_bottom;
  if (right <= left || bottom <= top) {
    return 0;
  }
  int width;
  int height;
  if (!Int64ToInt((int64_t)right - (int64_t)left, &width) ||
      !Int64ToInt((int64_t)bottom - (int64_t)top, &height)) {
    return 0;
  }

  *out = (Rect){
      .x = left,
      .y = top,
      .w = width,
      .h = height,
  };
  return 1;
}

int RectContainsPoint(Rect rect, int x, int y) {
  int right;
  int bottom;
  return !RectIsEmpty(rect) && RectEndChecked(rect.x, rect.w, &right) &&
         RectEndChecked(rect.y, rect.h, &bottom) && x >= rect.x && x < right &&
         y >= rect.y && y < bottom;
}

size_t RectSubtract(Rect old_rect, Rect new_rect, Rect out[4]) {
  if (RectIsEmpty(old_rect)) {
    return 0;
  }

  Rect intersection;
  if (!RectClip(old_rect, new_rect, &intersection)) {
    out[0] = old_rect;
    return 1;
  }

  size_t count = 0;
  int64_t old_right = RectRight(old_rect);
  int64_t old_bottom = RectBottom(old_rect);
  int64_t intersection_right = RectRight(intersection);
  int64_t intersection_bottom = RectBottom(intersection);

  if (intersection.y > old_rect.y) {
    out[count++] = (Rect){
        .x = old_rect.x,
        .y = old_rect.y,
        .w = old_rect.w,
        .h = intersection.y - old_rect.y,
    };
  }

  if (intersection_bottom < old_bottom) {
    out[count++] = (Rect){
        .x = old_rect.x,
        .y = (int)intersection_bottom,
        .w = old_rect.w,
        .h = (int)(old_bottom - intersection_bottom),
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

  if (intersection_right < old_right) {
    out[count++] = (Rect){
        .x = (int)intersection_right,
        .y = intersection.y,
        .w = (int)(old_right - intersection_right),
        .h = intersection.h,
    };
  }

  return count;
}
