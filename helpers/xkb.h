#ifndef XSECURELOCK_HELPERS_XKB_H
#define XSECURELOCK_HELPERS_XKB_H

#include <stddef.h>

#include <X11/Xlib.h>

#define XKB_INDICATOR_TEXT_SIZE 128

struct XkbIndicators {
  char text[XKB_INDICATOR_TEXT_SIZE];
  int warning;
  int have_multiple_layouts;
};

struct XkbIndicatorFormatInput {
  const char *layout_name;
  unsigned int implicit_mods;
  const char *const *indicator_names;
  size_t indicator_count;
  int show_keyboard_layout;
  int show_locks_and_latches;
  int have_multiple_layouts;
};

int HaveXkbExtension(Display *display);
int GetXkbIndicators(Display *display, int have_xkb_ext,
                     int show_keyboard_layout, int show_locks_and_latches,
                     struct XkbIndicators *result);
int FormatXkbIndicatorText(const struct XkbIndicatorFormatInput *input,
                           struct XkbIndicators *result);
void SwitchToNextXkbLayout(Display *display, int have_xkb_ext);

#endif  // XSECURELOCK_HELPERS_XKB_H
