#ifndef RECT_H
#define RECT_H

#include <stddef.h>

typedef struct {
  int x;
  int y;
  int w;
  int h;
} Rect;

size_t RectSubtract(Rect old_rect, Rect new_rect, Rect out[4]);

#endif
