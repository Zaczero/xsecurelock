#ifndef RECT_H
#define RECT_H

#include <stddef.h>

typedef struct {
  int x;
  int y;
  int w;
  int h;
} Rect;

int RectClip(Rect rect, Rect clip, Rect *out);
int RectContainsPoint(Rect rect, int x, int y);
size_t RectSubtract(Rect old_rect, Rect new_rect, Rect out[4]);

#endif
