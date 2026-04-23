#ifndef XSECURELOCK_HELPERS_XKB_H
#define XSECURELOCK_HELPERS_XKB_H

#include <stdbool.h>
#include <stddef.h>

#include <X11/Xlib.h>

#define XKB_INDICATOR_TEXT_SIZE 128

struct XkbIndicators {
  char text[XKB_INDICATOR_TEXT_SIZE];
  bool warning;
  bool have_multiple_layouts;
};

struct XkbIndicatorFormatInput {
  const char *layout_name;
  unsigned int implicit_mods;
  const char *const *indicator_names;
  size_t indicator_count;
  bool show_keyboard_layout;
  bool show_locks_and_latches;
  bool have_multiple_layouts;
};

int HaveXkbExtension(Display *display);
int GetXkbIndicators(Display *display, bool have_xkb_ext,
                     bool show_keyboard_layout, bool show_locks_and_latches,
                     struct XkbIndicators *result);
int FormatXkbIndicatorText(const struct XkbIndicatorFormatInput *input,
                           struct XkbIndicators *result);
void SwitchToNextXkbLayout(Display *display, bool have_xkb_ext);

#endif  // XSECURELOCK_HELPERS_XKB_H
