#include "config.h"

#include "auth_draw.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_XFT_EXT
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#endif

#include "../time_util.h"
#include "../util.h"
#include "auth_title.h"
#include "auth_windows.h"
#include "xkb.h"

#define LINE_SPACING 4
#define NOTE_DS3 156
#define NOTE_A3 220
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_B4 494
#define NOTE_E5 659
#define SOUND_SLEEP_MS 125
#define SOUND_TONE_MS 100

static const int kSounds[][2] = {
    /* AUTH_SOUND_PROMPT=  */ {NOTE_B4, NOTE_E5},
    /* AUTH_SOUND_INFO=    */ {NOTE_E5, NOTE_E5},
    /* AUTH_SOUND_ERROR=   */ {NOTE_A3, NOTE_DS3},
    /* AUTH_SOUND_SUCCESS= */ {NOTE_DS4, NOTE_E4},
};
XSL_STATIC_ASSERT(ARRAY_LEN(kSounds) == AUTH_SOUND_SUCCESS + 1,
                  "Auth sound table must match enum AuthSound");

static int TextAscent(const struct AuthUiContext *ctx) {
#ifdef HAVE_XFT_EXT
  if (ctx->resources.xft_font != NULL) {
    return ctx->resources.xft_font->ascent;
  }
#endif
  return ctx->resources.core_font->max_bounds.ascent;
}

static int TextDescent(const struct AuthUiContext *ctx) {
#ifdef HAVE_XFT_EXT
  if (ctx->resources.xft_font != NULL) {
    return ctx->resources.xft_font->descent;
  }
#endif
  return ctx->resources.core_font->max_bounds.descent;
}

#ifdef HAVE_XFT_EXT
static int XGlyphInfoExpandAmount(XGlyphInfo *extents) {
  int expand_left = extents->x;
  int expand_right = -extents->x + extents->width - extents->xOff;
  int expand_max = expand_left > expand_right ? expand_left : expand_right;
  return expand_max > 0 ? expand_max : 0;
}
#endif

static int TextWidth(const struct AuthUiContext *ctx, const char *string,
                     int len) {
#ifdef HAVE_XFT_EXT
  if (ctx->resources.xft_font != NULL) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(ctx->resources.display, ctx->resources.xft_font,
                       (const FcChar8 *)string, len, &extents);
    return extents.xOff + 2 * XGlyphInfoExpandAmount(&extents);
  }
#endif
  return XTextWidth(ctx->resources.core_font, string, len);
}

static void DrawString(const struct AuthUiContext *ctx, size_t window_index,
                       int x, int y, bool warning, const char *string,
                       int len) {
#ifdef HAVE_XFT_EXT
  if (ctx->resources.xft_font != NULL) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(ctx->resources.display, ctx->resources.xft_font,
                       (const FcChar8 *)string, len, &extents);
    XftDrawStringUtf8(ctx->windows.xft_draws[window_index],
                      warning ? &ctx->resources.xft_color_warning
                              : &ctx->resources.xft_color_foreground,
                      ctx->resources.xft_font,
                      x + XGlyphInfoExpandAmount(&extents), y,
                      (const FcChar8 *)string, len);
    return;
  }
#endif
  XDrawString(ctx->resources.display, ctx->windows.windows[window_index],
              warning ? ctx->windows.warning_gcs[window_index]
                      : ctx->windows.gcs[window_index],
              x, y, string, len);
}

static int AuthDialogInset(const struct AuthUiContext *ctx) {
  return ctx->config.auth_padding + ctx->config.auth_border_size;
}

static void DrawDialogBorder(const struct AuthUiContext *ctx,
                             size_t window_index, int region_w, int region_h) {
  if (ctx->config.auth_border_size <= 0) {
    return;
  }

  int rect_w = region_w - ctx->config.auth_border_size - 1;
  int rect_h = region_h - ctx->config.auth_border_size - 1;
  if (rect_w <= 0 || rect_h <= 0) {
    return;
  }

  XSetLineAttributes(ctx->resources.display, ctx->windows.gcs[window_index],
                     ctx->config.auth_border_size, LineSolid, CapButt,
                     JoinMiter);
  int border_offset = ctx->config.auth_border_size / 2;
  XDrawRectangle(ctx->resources.display, ctx->windows.windows[window_index],
                 ctx->windows.gcs[window_index], border_offset, border_offset,
                 rect_w, rect_h);
}

enum { AUTH_MESSAGE_MAX_ROWS = 6 };

struct AuthTextRow {
  const char *text;
  int len;
  int width;
  bool warning;
  int advance_lines;
};

static struct AuthTextRow MeasureTextRow(const struct AuthUiContext *ctx,
                                         const char *text, bool warning,
                                         int advance_lines) {
  int len = (int)strlen(text);
  return (struct AuthTextRow){
      .text = text,
      .len = len,
      .width = TextWidth(ctx, text, len),
      .warning = warning,
      .advance_lines = advance_lines,
  };
}

static int MaxMessageRowWidth(const struct AuthTextRow *rows,
                              size_t row_count) {
  int max_width = 0;

  for (size_t i = 0; i < row_count; ++i) {
    if (max_width < rows[i].width) {
      max_width = rows[i].width;
    }
  }
  return max_width;
}

static int TotalMessageRowHeight(const struct AuthTextRow *rows,
                                 size_t row_count, int line_height) {
  int total_height = 0;

  for (size_t i = 0; i < row_count; ++i) {
    total_height += rows[i].advance_lines * line_height;
  }
  return total_height;
}

void AuthPlaySound(struct AuthUiContext *ctx, enum AuthSound sound) {
  if (!ctx->config.auth_sounds) {
    return;
  }

  XKeyboardState state;
  XKeyboardControl control;
  XGetKeyboardControl(ctx->resources.display, &state);

  control.bell_percent = 50;
  control.bell_duration = SOUND_TONE_MS;
  control.bell_pitch = kSounds[sound][0];
  XChangeKeyboardControl(ctx->resources.display,
                         KBBellPercent | KBBellDuration | KBBellPitch,
                         &control);
  XBell(ctx->resources.display, 0);
  XFlush(ctx->resources.display);
  (void)SleepMs(SOUND_SLEEP_MS);

  control.bell_pitch = kSounds[sound][1];
  XChangeKeyboardControl(ctx->resources.display, KBBellPitch, &control);
  XBell(ctx->resources.display, 0);

  control.bell_percent = state.bell_percent;
  control.bell_duration = state.bell_duration;
  control.bell_pitch = state.bell_pitch;
  XChangeKeyboardControl(ctx->resources.display,
                         KBBellPercent | KBBellDuration | KBBellPitch,
                         &control);
  XFlush(ctx->resources.display);
  (void)SleepMs(SOUND_SLEEP_MS);
}

int AuthDisplayMessage(struct AuthUiContext *ctx, const char *title,
                       const char *message, bool warning) {
  char full_title[256];
  char datetime[80] = "";
  struct XkbIndicators indicators = {0};
  struct AuthTextRow rows[AUTH_MESSAGE_MAX_ROWS];
  size_t row_count = 0;

  AuthBuildTitle(full_title, sizeof(full_title), ctx->config.auth_title,
                 ctx->config.show_username, ctx->config.show_hostname,
                 ctx->config.username, ctx->config.hostname, title);

  (void)GetXkbIndicators(ctx->resources.display, ctx->resources.have_xkb_ext,
                         ctx->config.show_keyboard_layout,
                         ctx->config.show_locks_and_latches, &indicators);

  if (ctx->config.show_datetime) {
    time_t rawtime = 0;

    if (time(&rawtime) != (time_t)-1) {
      struct tm timeinfo_buf;
      struct tm *timeinfo = localtime_r(&rawtime, &timeinfo_buf);
      if (timeinfo != NULL &&
          strftime(datetime, sizeof(datetime), ctx->config.datetime_format,
                   timeinfo) == 0) {
        datetime[0] = '\0';
      }
    }
  }

  if (ctx->config.show_datetime) {
    rows[row_count++] = MeasureTextRow(ctx, datetime, false, 2);
  }
  rows[row_count++] = MeasureTextRow(ctx, full_title, warning, 2);
  rows[row_count++] = MeasureTextRow(ctx, message, warning, 1);
  rows[row_count++] =
      MeasureTextRow(ctx, indicators.text, indicators.warning != 0, 1);
  if (indicators.have_multiple_layouts) {
    char layout_switch_hint[96];
    (void)snprintf(layout_switch_hint, sizeof(layout_switch_hint),
                   "Press Ctrl-%s to switch keyboard layout",
                   ctx->config.layout_switch_key_name);
    rows[row_count++] = MeasureTextRow(ctx, layout_switch_hint, false, 1);
  }
  if (ctx->config.have_switch_user_command) {
    rows[row_count++] = MeasureTextRow(
        ctx, "Press Ctrl-Alt-O or Win-O to switch user", false, 0);
  }

  int line_height = TextAscent(ctx) + TextDescent(ctx) + LINE_SPACING;
  int baseline_offset = TextAscent(ctx) + LINE_SPACING / 2;
  int box_h = TotalMessageRowHeight(rows, row_count, line_height);
  int region_w = MaxMessageRowWidth(rows, row_count) + 2 * AuthDialogInset(ctx);
  int region_h = box_h + 2 * AuthDialogInset(ctx);

  if (ctx->config.burnin_mitigation_max_offset_change > 0) {
    ctx->runtime.x_offset =
        StepBurnInOffset(&ctx->runtime.prompt_rng, ctx->runtime.x_offset,
                         ctx->config.burnin_mitigation_max_offset,
                         ctx->config.burnin_mitigation_max_offset_change);
    ctx->runtime.y_offset =
        StepBurnInOffset(&ctx->runtime.prompt_rng, ctx->runtime.y_offset,
                         ctx->config.burnin_mitigation_max_offset,
                         ctx->config.burnin_mitigation_max_offset_change);
  }

  if (!AuthWindowsUpdate(ctx, region_w, region_h)) {
    ctx->windows.dirty = true;
    return 0;
  }
  ctx->windows.dirty = false;

  for (size_t i = 0; i < ctx->windows.count; ++i) {
    int cx = region_w / 2;
    int y = region_h / 2 + baseline_offset - box_h / 2;

    XClearWindow(ctx->resources.display, ctx->windows.windows[i]);
    DrawDialogBorder(ctx, i, region_w, region_h);

    for (size_t j = 0; j < row_count; ++j) {
      DrawString(ctx, i, cx - rows[j].width / 2, y, rows[j].warning,
                 rows[j].text, rows[j].len);
      y += rows[j].advance_lines * line_height;
    }
  }

  XFlush(ctx->resources.display);
  return 1;
}
