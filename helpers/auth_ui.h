#ifndef XSECURELOCK_HELPERS_AUTH_UI_H
#define XSECURELOCK_HELPERS_AUTH_UI_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef HAVE_XFT_EXT
#include <X11/Xft/Xft.h>
#endif

#include "../rect.h"
#include "monitors.h"
#include "prompt_display.h"
#include "prompt_random.h"

#define AUTH_UI_MAX_WINDOWS 16
#define AUTH_UI_MAIN_WINDOW 0
#define AUTH_UI_HOSTNAME_BUFFER_SIZE 256
#define AUTH_UI_USERNAME_BUFFER_SIZE 256

enum AuthSound {
  AUTH_SOUND_PROMPT,
  AUTH_SOUND_INFO,
  AUTH_SOUND_ERROR,
  AUTH_SOUND_SUCCESS,
};

struct AuthUiConfig {
  int argc;
  char *const *argv;
  const char *authproto_executable;
  int prompt_timeout;
  enum PromptDisplayMode prompt_display_mode;
  bool have_switch_user_command;
  const char *auth_title;
  bool show_username;
  int show_hostname;
  bool show_datetime;
  const char *datetime_format;
  const char *background_color;
  const char *foreground_color;
  const char *warning_color;
  const char *font_name;
  bool auth_sounds;
  bool auth_cursor_blink;
  int auth_padding;
  int auth_border_size;
  int auth_x_position;
  int auth_y_position;
  bool single_auth_window;
  int burnin_mitigation_max_offset;
  int burnin_mitigation_max_offset_change;
  bool show_keyboard_layout;
  bool show_locks_and_latches;
  const char *layout_switch_key_name;
  char hostname[AUTH_UI_HOSTNAME_BUFFER_SIZE];
  char username[AUTH_UI_USERNAME_BUFFER_SIZE];
};

struct AuthUiResources {
  Display *display;
  Window main_window;
  Window parent_window;
  Colormap colormap;
  XFontStruct *core_font;
  XColor xcolor_background;
  XColor xcolor_foreground;
  XColor xcolor_warning;
  bool background_color_allocated;
  bool foreground_color_allocated;
  bool warning_color_allocated;
  bool have_xkb_ext;
#ifdef HAVE_XFT_EXT
  XftColor xft_color_foreground;
  XftColor xft_color_warning;
  XftFont *xft_font;
  bool xft_color_foreground_allocated;
  bool xft_color_warning_allocated;
#endif
};

struct AuthWindowSet {
  bool dirty;
  size_t count;
  size_t monitor_count;
  Window windows[AUTH_UI_MAX_WINDOWS];
  Rect rects[AUTH_UI_MAX_WINDOWS];
  GC gcs[AUTH_UI_MAX_WINDOWS];
  GC warning_gcs[AUTH_UI_MAX_WINDOWS];
  Monitor monitors[AUTH_UI_MAX_WINDOWS];
#ifdef HAVE_XFT_EXT
  XftDraw *xft_draws[AUTH_UI_MAX_WINDOWS];
#endif
};

struct AuthUiRuntime {
  struct PromptRng prompt_rng;
  int x_offset;
  int y_offset;
};

struct AuthUiContext {
  struct AuthUiConfig config;
  struct AuthUiResources resources;
  struct AuthWindowSet windows;
  struct AuthUiRuntime runtime;
};

void AuthUiContextInit(struct AuthUiContext *ctx, int argc, char **argv);

#endif  // XSECURELOCK_HELPERS_AUTH_UI_H
