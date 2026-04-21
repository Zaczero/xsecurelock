/*
Copyright 2014 Google Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#include "config.h"
#include "build-config.h"

#include <X11/X.h>     // for Success, None, Atom, KBBellPitch
#include <X11/Xlib.h>  // for DefaultScreen, Screen, XFree, True
#include <errno.h>     // for errno, ESRCH
#include <locale.h>    // for NULL, setlocale, LC_CTYPE, LC_TIME
#include <poll.h>      // for pollfd, POLLIN, POLLHUP, POLLNVAL
#include <signal.h>    // for SIGTERM, kill
#include <stdio.h>
#include <stdlib.h>      // for free, mblen, size_t, EXIT_FAILURE
#include <string.h>      // for strlen, memcpy, memset, strcspn
#include <sys/time.h>    // for gettimeofday, timeval
#include <time.h>        // for time, nanosleep, localtime_r
#include <unistd.h>      // for close, _exit, dup2, dup

#if __STDC_VERSION__ >= 199901L
#include <inttypes.h>
#include <stdint.h>
#endif

#ifdef HAVE_XFT_EXT
#include <X11/Xft/Xft.h>             // for XftColorAllocValue, XftColorFree
#include <X11/extensions/Xrender.h>  // for XRenderColor, XGlyphInfo
#include <fontconfig/fontconfig.h>   // for FcChar8
#endif

#ifdef HAVE_XKB_EXT
#include <X11/XKBlib.h>             // for XkbFreeKeyboard, XkbGetControls
#include <X11/extensions/XKB.h>     // for XkbUseCoreKbd, XkbGroupsWrapMask
#include <X11/extensions/XKBstr.h>  // for _XkbDesc, XkbStateRec, _XkbControls
#endif

#include "../env_info.h"          // for GetHostName, GetUserName
#include "../env_settings.h"      // for GetIntSetting, GetStringSetting
#include "../logging.h"           // for Log, LogErrno
#include "../mlock_page.h"        // for MLOCK_PAGE
#include "../rect.h"              // for Rect, RectSubtract
#include "../util.h"              // for explicit_bzero
#include "../wait_pgrp.h"         // for WaitPgrp
#include "../wm_properties.h"     // for SetWMProperties
#include "../xscreensaver_api.h"  // for ReadWindowID
#include "authproto.h"            // for WritePacket, ReadPacket, PTYPE_R...
#include "indicator_text.h"       // for AppendIndicatorText
#include "monitors.h"             // for Monitor, GetMonitors, IsMonitorC...
#include "prompt_display.h"       // for DISCO_PASSWORD_DANCERS, Format...
#include "prompt_random.h"        // for PromptRng, NextDisplayMarker

#if __STDC_VERSION__ >= 201112L
#define STATIC_ASSERT(state, message) _Static_assert(state, message)
#else
#define STATIC_ASSERT(state, message) \
  extern int statically_asserted(int assertion[(state) ? 1 : -1]);
#endif

//! Number of args.
static int argc;

//! Args.
static char *const *argv;

//! The authproto helper to use.
static const char *authproto_executable;

//! The blinking interval in microseconds.
#define BLINK_INTERVAL (250 * 1000)

//! The maximum time to wait at a prompt for user input in seconds.
static int prompt_timeout;

//! Length of the "paranoid password display".
#define PARANOID_PASSWORD_LENGTH (1 << DISCO_PASSWORD_DANCERS)

//! Minimum distance the cursor shall move on keypress.
#define PARANOID_PASSWORD_MIN_CHANGE 4
STATIC_ASSERT(PARANOID_PASSWORD_MIN_CHANGE <=
                  (PARANOID_PASSWORD_LENGTH - 2) / 2,
              "Display marker movement must always leave a valid next choice");

//! Extra line spacing.
#define LINE_SPACING 4

//! Actual password prompt selected
enum PasswordPrompt {
  PASSWORD_PROMPT_CURSOR,
  PASSWORD_PROMPT_ASTERISKS,
  PASSWORD_PROMPT_HIDDEN,
  PASSWORD_PROMPT_DISCO,
  PASSWORD_PROMPT_EMOJI,
  PASSWORD_PROMPT_EMOTICON,
  PASSWORD_PROMPT_KAOMOJI,
#if __STDC_VERSION__ >= 199901L
  PASSWORD_PROMPT_TIME,
  PASSWORD_PROMPT_TIME_HEX,
#endif

  PASSWORD_PROMPT_COUNT,
};
static const char *const PasswordPromptStrings[] = {
    /* PASSWORD_PROMPT_CURSOR= */ "cursor",
    /* PASSWORD_PROMPT_ASTERISKS= */ "asterisks",
    /* PASSWORD_PROMPT_HIDDEN= */ "hidden",
    /* PASSWORD_PROMPT_DISCO= */ "disco",
    /* PASSWORD_PROMPT_EMOJI= */ "emoji",
    /* PASSWORD_PROMPT_EMOTICON= */ "emoticon",
    /* PASSWORD_PROMPT_KAOMOJI= */ "kaomoji",
#if __STDC_VERSION__ >= 199901L
    /* PASSWORD_PROMPT_TIME= */ "time",
    /* PASSWORD_PROMPT_TIME_HEX= */ "time_hex",
#endif
};

static enum PasswordPrompt password_prompt;

// Emoji to display in emoji mode. The length of the array must be equal to
// PARANOID_PASSWORD_LENGTH. List taken from the top items in
// http://emojitracker.com/ The first item is always display in an empty prompt
// (before typing in the password)
static const char *const emoji[] = {
    "_____", "😂", "❤", "♻", "😍", "♥", "😭", "😊", "😒", "💕", "😘",
    "😩",     "☺", "👌", "😔", "😁", "😏", "😉", "👍", "⬅", "😅", "🙏",
    "😌",     "😢", "👀", "💔", "😎", "🎶", "💙", "💜", "🙌", "😳",
};
STATIC_ASSERT(sizeof(emoji) / sizeof(*emoji) == PARANOID_PASSWORD_LENGTH,
              "Emoji array size must be equal to PARANOID_PASSWORD_LENGTH");

// Emoticons to display in emoji mode. The length of the array must be equal to
// PARANOID_PASSWORD_LENGTH. The first item is always display in an empty prompt
// (before typing in the password)
static const char *const emoticons[] = {
    ":-)",  ":-p", ":-O", ":-\\", "(-:",  "d-:", "O-:", "/-:",
    "8-)",  "8-p", "8-O", "8-\\", "(-8",  "d-8", "O-8", "/-8",
    "X-)",  "X-p", "X-O", "X-\\", "(-X",  "d-X", "O-X", "/-X",
    ":'-)", ":-S", ":-D", ":-#",  "(-':", "S-:", "D-:", "#-:",
};
STATIC_ASSERT(sizeof(emoticons) / sizeof(*emoticons) ==
                  PARANOID_PASSWORD_LENGTH,
              "Emoticons array size must be equal to PARANOID_PASSWORD_LENGTH");

// Kaomoji to display in kaomoji mode. The length of the array must be equal to
// PARANOID_PASSWORD_LENGTH. The first item is always display in an empty prompt
// (before typing in the password)
static const char *const kaomoji[] = {
    "(͡°͜ʖ͡°)",     "(>_<)",       "O_ם",      "(^_-)",        "o_0",
    "o.O",       "0_o",         "O.o",      "(°o°)",        "^m^",
    "^_^",       "((d[-_-]b))", "┏(･o･)┛",  "┗(･o･)┓",      "（ﾟДﾟ)",
    "(°◇°)",     "\\o/",        "\\o|",     "|o/",          "|o|",
    "(●＾o＾●)", "(＾ｖ＾)",    "(＾ｕ＾)", "(＾◇＾)",      "¯\\_(ツ)_/¯",
    "(^0_0^)",   "(☞ﾟ∀ﾟ)☞",     "(-■_■)",   "(┛ಠ_ಠ)┛彡┻━┻", "┬─┬ノ(º_ºノ)",
    "(˘³˘)♥",    "❤(◍•ᴗ•◍)",
};
STATIC_ASSERT(sizeof(kaomoji) / sizeof(*kaomoji) == PARANOID_PASSWORD_LENGTH,
              "Kaomoji array size must be equal to PARANOID_PASSWORD_LENGTH");

//! If set, we can start a new login session.
static int have_switch_user_command;

//! If set, the prompt will be fixed by <username>@.
static int show_username;

//! If set, the prompt will be fixed by <hostname>. If >1, the hostname will be
// shown in full and not cut at the first dot.
static int show_hostname;

//! If set, data and time will be shown.
static int show_datetime;

//! The date format to display.
static const char *datetime_format = "%c";

//! The local hostname.
static char hostname[256];

//! The username to authenticate as.
static char username[256];

//! The X11 display.
static Display *display;

//! The X11 window provided by main. Provided from $XSCREENSAVER_WINDOW.
static Window main_window;

//! main_window's parent. Used to create per-monitor siblings.
static Window parent_window;

//! The X11 core font for the PAM messages.
static XFontStruct *core_font;

#ifdef HAVE_XFT_EXT
//! The Xft font for the PAM messages.
static XftColor xft_color_foreground;
static XftColor xft_color_warning;
static XftFont *xft_font;
static int xft_color_foreground_allocated = 0;
static int xft_color_warning_allocated = 0;
#endif

//! The background color.
static XColor xcolor_background;

//! The foreground color.
static XColor xcolor_foreground;

//! The warning color (used as foreground).
static XColor xcolor_warning;

//! The cursor character displayed at the end of the masked password input.
static const char cursor[] = "_";

//! The x offset to apply to the entire display (to mitigate burn-in).
static int x_offset = 0;

//! The y offset to apply to the entire display (to mitigate burn-in).
static int y_offset = 0;

//! Local UI RNG state for display marker and burn-in mitigation jitter only.
static struct PromptRng prompt_rng;

//! Maximum offset value when dynamic changes are enabled.
static int burnin_mitigation_max_offset = 0;

//! How much the offsets are allowed to change dynamically, and if so, how high.
static int burnin_mitigation_max_offset_change = 0;

//! Whether to play sounds during authentication.
static int auth_sounds = 0;

//! Whether to blink the cursor in the auth dialog.
static int auth_cursor_blink = 1;

//! Padding between auth dialog content and its optional border.
static int auth_padding = 16;

//! Border stroke width for the auth dialog. Zero disables the border.
static int auth_border_size = 0;

//! Horizontal position of the auth dialog as a percentage (0=left, 50=center,
//! 100=right).
static int auth_x_position = 50;

//! Vertical position of the auth dialog as a percentage (0=top, 50=center,
//! 100=bottom).
static int auth_y_position = 50;

//! Whether we only want a single auth window.
static int single_auth_window = 0;

//! If set, we need to re-query monitor data and adjust windows.
static int per_monitor_windows_dirty = 1;

#ifdef HAVE_XKB_EXT
//! If set, we show Xkb keyboard layout name.
static int show_keyboard_layout = 1;
//! If set, we show Xkb lock/latch status rather than Xkb indicators.
static int show_locks_and_latches = 0;
#endif

#define MAIN_WINDOW 0
#define MAX_WINDOWS 16

//! The number of active X11 per-monitor windows.
static size_t num_windows = 0;

//! The X11 per-monitor windows to draw on.
static Window windows[MAX_WINDOWS];

//! The last known geometry for each per-monitor window.
static Rect window_rects[MAX_WINDOWS];

//! The X11 graphics contexts to draw with.
static GC gcs[MAX_WINDOWS];

//! The X11 graphics contexts to draw warnings with.
static GC gcs_warning[MAX_WINDOWS];

#ifdef HAVE_XFT_EXT
//! The Xft draw contexts to draw with.
static XftDraw *xft_draws[MAX_WINDOWS];
#endif

static int have_xkb_ext;

static void FreeKeyboardDescription(XkbDescPtr *xkb) {
#ifdef HAVE_XKB_EXT
  if (*xkb != NULL) {
    XkbFreeKeyboard(*xkb, 0, True);
    *xkb = NULL;
  }
#else
  (void)xkb;
#endif
}

static XkbDescPtr GetKeyboardDescription(unsigned int names_mask) {
#ifdef HAVE_XKB_EXT
  XkbDescPtr xkb = XkbGetMap(display, 0, XkbUseCoreKbd);
  if (xkb == NULL) {
    Log("XkbGetMap failed");
    return NULL;
  }
  if (XkbGetControls(display, XkbGroupsWrapMask, xkb) != Success) {
    Log("XkbGetControls failed");
    FreeKeyboardDescription(&xkb);
    return NULL;
  }
  if (xkb->ctrls == NULL) {
    Log("XkbGetControls returned no controls");
    FreeKeyboardDescription(&xkb);
    return NULL;
  }
  if (names_mask != 0) {
    if (XkbGetNames(display, names_mask, xkb) != Success) {
      Log("XkbGetNames failed");
      FreeKeyboardDescription(&xkb);
      return NULL;
    }
    if (xkb->names == NULL) {
      Log("XkbGetNames returned no names");
      FreeKeyboardDescription(&xkb);
      return NULL;
    }
  }
  return xkb;
#else
  (void)names_mask;
  return NULL;
#endif
}

static int GetKeyboardState(XkbStateRec *state) {
#ifdef HAVE_XKB_EXT
  if (XkbGetState(display, XkbUseCoreKbd, state) != Success) {
    Log("XkbGetState failed");
    return 0;
  }
  return 1;
#else
  (void)state;
  return 0;
#endif
}

static char *GetAtomNameOrNull(Atom atom) {
  if (atom == None) {
    return NULL;
  }
  char *name = XGetAtomName(display, atom);
  if (name == NULL) {
    Log("XGetAtomName failed for atom %#lx", (unsigned long)atom);
  }
  return name;
}

static int AppendIndicatorAtomName(char **output, size_t *output_size,
                                   int *have_output, Atom atom) {
  char *name = GetAtomNameOrNull(atom);
  if (name == NULL) {
    return 1;
  }
  int ok = AppendIndicatorText(output, output_size, have_output, name);
  if (!ok) {
    Log("Not enough space to store modifier name '%s'", name);
  }
  XFree(name);
  return ok;
}

static int AllocNamedColorOrLog(const char *label, const char *color_name,
                                XColor *color) {
  XColor dummy;
  if (XAllocNamedColor(display, DefaultColormap(display, DefaultScreen(display)),
                       color_name, color, &dummy)) {
    return 1;
  }
  Log("Could not allocate %s color '%s'", label, color_name);
  return 0;
}

#ifdef HAVE_XFT_EXT
static int AllocXftColorOrLog(const char *label, const XColor *xcolor,
                              XftColor *xft_color, int *allocated) {
  XRenderColor xrcolor;
  xrcolor.alpha = 65535;
  xrcolor.red = xcolor->red;
  xrcolor.green = xcolor->green;
  xrcolor.blue = xcolor->blue;
  if (!XftColorAllocValue(display, DefaultVisual(display, DefaultScreen(display)),
                          DefaultColormap(display, DefaultScreen(display)),
                          &xrcolor, xft_color)) {
    Log("XftColorAllocValue failed for %s color", label);
    return 0;
  }
  *allocated = 1;
  return 1;
}
#endif

enum Sound { SOUND_PROMPT, SOUND_INFO, SOUND_ERROR, SOUND_SUCCESS };

#define NOTE_DS3 156
#define NOTE_A3 220
#define NOTE_DS4 311
#define NOTE_E4 330
#define NOTE_B4 494
#define NOTE_E5 659
static const int sounds[][2] = {
    /* SOUND_PROMPT=  */ {NOTE_B4, NOTE_E5},   // V|I I
    /* SOUND_INFO=    */ {NOTE_E5, NOTE_E5},   // I 2x
    /* SOUND_ERROR=   */ {NOTE_A3, NOTE_DS3},  // V7 2x
    /* SOUND_SUCCESS= */ {NOTE_DS4, NOTE_E4},  // V I
};
#define SOUND_SLEEP_MS 125
#define SOUND_TONE_MS 100

/*! \brief Play a sound sequence.
 */
static void PlaySound(enum Sound snd) {
  XKeyboardState state;
  XKeyboardControl control;
  struct timespec sleeptime;

  if (!auth_sounds) {
    return;
  }

  XGetKeyboardControl(display, &state);

  // bell_percent changes note length on Linux, so let's use the middle value
  // to get a 1:1 mapping.
  control.bell_percent = 50;
  control.bell_duration = SOUND_TONE_MS;
  control.bell_pitch = sounds[snd][0];
  XChangeKeyboardControl(display, KBBellPercent | KBBellDuration | KBBellPitch,
                         &control);
  XBell(display, 0);

  XFlush(display);

  sleeptime.tv_sec = SOUND_SLEEP_MS / 1000;
  sleeptime.tv_nsec = 1000000L * (SOUND_SLEEP_MS % 1000);
  nanosleep(&sleeptime, NULL);

  control.bell_pitch = sounds[snd][1];
  XChangeKeyboardControl(display, KBBellPitch, &control);
  XBell(display, 0);

  control.bell_percent = state.bell_percent;
  control.bell_duration = state.bell_duration;
  control.bell_pitch = state.bell_pitch;
  XChangeKeyboardControl(display, KBBellPercent | KBBellDuration | KBBellPitch,
                         &control);

  XFlush(display);

  nanosleep(&sleeptime, NULL);
}

/*! \brief Switch to the next keyboard layout.
 */
static void SwitchKeyboardLayout(void) {
#ifdef HAVE_XKB_EXT
  if (!have_xkb_ext) {
    return;
  }

  XkbDescPtr xkb = GetKeyboardDescription(0);
  if (xkb == NULL) {
    return;
  }
  if (xkb->ctrls->num_groups < 1) {
    Log("XkbGetControls returned less than 1 group");
    FreeKeyboardDescription(&xkb);
    return;
  }
  XkbStateRec state;
  if (!GetKeyboardState(&state)) {
    FreeKeyboardDescription(&xkb);
    return;
  }

  XkbLockGroup(display, XkbUseCoreKbd,
               (state.group + 1) % xkb->ctrls->num_groups);

  FreeKeyboardDescription(&xkb);
#endif
}

/*! \brief Check which modifiers are active.
 *
 * \param warning Will be set to 1 if something's "bad" with the keyboard
 *     layout (e.g. Caps Lock).
 * \param have_multiple_layouts Will be set to 1 if more than one keyboard
 *     layout is available for switching.
 *
 * \return The current modifier mask as a string.
 */
static const char *GetIndicators(int *warning, int *have_multiple_layouts) {
#ifdef HAVE_XKB_EXT
  static char buf[128];

  if (!have_xkb_ext) {
    return "";
  }

  XkbDescPtr xkb =
      GetKeyboardDescription(XkbIndicatorNamesMask | XkbGroupNamesMask |
                             XkbSymbolsNameMask);
  if (xkb == NULL) {
    return "";
  }
  XkbStateRec state;
  if (!GetKeyboardState(&state)) {
    FreeKeyboardDescription(&xkb);
    return "";
  }
  unsigned int istate = 0;
  if (!show_locks_and_latches) {
    if (XkbGetIndicatorState(display, XkbUseCoreKbd, &istate) != Success) {
      Log("XkbGetIndicatorState failed");
      FreeKeyboardDescription(&xkb);
      return "";
    }
  }

  // Detect Caps Lock.
  // Note: in very pathological cases the modifier might be set without an
  // XkbIndicator for it; then we show the line in red without telling the user
  // why. Such a situation has not been observd yet though.
  unsigned int implicit_mods = state.latched_mods | state.locked_mods;
  if (implicit_mods & LockMask) {
    *warning = 1;
  }

  // Provide info about multiple layouts.
  if (xkb->ctrls->num_groups > 1) {
    *have_multiple_layouts = 1;
  }

  char *p = buf;
  size_t buf_remaining = sizeof(buf);
  buf[0] = 0;

  const char *word = "Keyboard: ";
  size_t n = strlen(word);
  if (n + 1 > buf_remaining) {
    Log("Not enough space to store intro '%s'", word);
    FreeKeyboardDescription(&xkb);
    return "";
  }
  memcpy(p, word, n);
  p += n;
  buf_remaining -= n;
  *p = 0;

  int have_output = 0;
  if (show_keyboard_layout) {
    Atom layouta = None;
    if ((unsigned int)state.group < XkbNumKbdGroups) {
      layouta = xkb->names->groups[state.group];  // Human-readable.
    }
    if (layouta == None) {
      layouta = xkb->names->symbols;  // Machine-readable fallback.
    }
    if (!AppendIndicatorAtomName(&p, &buf_remaining, &have_output, layouta)) {
      FreeKeyboardDescription(&xkb);
      return "";
    }
  }

  if (show_locks_and_latches) {
    // TODO(divVerent): There must be a better way to get the names of the
    // modifiers than explicitly enumerating them. Also, there may even be
    // something that knows that Mod1 is Alt/Meta and Mod2 is Num lock.
#define APPEND_INDICATOR(mask, name)                                    \
    do {                                                                \
      if ((implicit_mods & (mask)) &&                                   \
          !AppendIndicatorText(&p, &buf_remaining, &have_output,        \
                               (name))) {                               \
        Log("Not enough space to store modifier name '%s'", (name));    \
        goto done;                                                      \
      }                                                                 \
    } while (0)
    APPEND_INDICATOR(ShiftMask, "Shift");
    APPEND_INDICATOR(LockMask, "Lock");
    APPEND_INDICATOR(ControlMask, "Control");
    APPEND_INDICATOR(Mod1Mask, "Mod1");
    APPEND_INDICATOR(Mod2Mask, "Mod2");
    APPEND_INDICATOR(Mod3Mask, "Mod3");
    APPEND_INDICATOR(Mod4Mask, "Mod4");
    APPEND_INDICATOR(Mod5Mask, "Mod5");
#undef APPEND_INDICATOR
  } else {
    for (int i = 0; i < XkbNumIndicators; i++) {
      if (!(istate & (1U << i))) {
        continue;
      }
      Atom namea = xkb->names->indicators[i];
      if (namea == None) {
        continue;
      }
      if (!AppendIndicatorAtomName(&p, &buf_remaining, &have_output, namea)) {
        break;
      }
    }
  }
done:
  *p = 0;
  FreeKeyboardDescription(&xkb);
  return have_output ? buf : "";
#else
  *warning = *warning;                              // Shut up clang-analyzer.
  *have_multiple_layouts = *have_multiple_layouts;  // Shut up clang-analyzer.
  return "";
#endif
}

static void CleanupPerMonitorWindow(size_t i) {
  if (windows[i] == None) {
    return;
  }
  // Once the window is unmapped or destroyed, nobody else is guaranteed to
  // repaint the pixels it used to cover immediately.
  XClearWindow(display, windows[i]);
#ifdef HAVE_XFT_EXT
  if (xft_draws[i] != NULL) {
    XftDrawDestroy(xft_draws[i]);
    xft_draws[i] = NULL;
  }
#endif
  if (gcs_warning[i] != NULL) {
    XFreeGC(display, gcs_warning[i]);
    gcs_warning[i] = NULL;
  }
  if (gcs[i] != NULL) {
    XFreeGC(display, gcs[i]);
    gcs[i] = NULL;
  }
  if (i == MAIN_WINDOW) {
    XUnmapWindow(display, windows[i]);
  } else {
    XDestroyWindow(display, windows[i]);
  }
  windows[i] = None;
}

static void DestroyPerMonitorWindows(size_t keep_windows) {
  for (size_t i = keep_windows; i < num_windows; ++i) {
    CleanupPerMonitorWindow(i);
  }
  if (num_windows > keep_windows) {
    num_windows = keep_windows;
  }
}

static void ClearWindowUncoveredAreas(size_t i, Rect new_rect) {
  Rect uncovered[4];
  size_t count = RectSubtract(window_rects[i], new_rect, uncovered);
  for (size_t j = 0; j < count; ++j) {
    XClearArea(display, windows[i], uncovered[j].x - window_rects[i].x,
               uncovered[j].y - window_rects[i].y, uncovered[j].w,
               uncovered[j].h, False);
  }
}

static int CreateOrUpdatePerMonitorWindow(size_t i, const Monitor *monitor,
                                          int region_w, int region_h,
                                          int x_offset, int y_offset) {
  // Desired box.
  int w = region_w;
  int h = region_h;
  int x = monitor->x + (monitor->width - w) * auth_x_position / 100 + x_offset;
  int y = monitor->y + (monitor->height - h) * auth_y_position / 100 + y_offset;
  // Clip to monitor.
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > monitor->x + monitor->width) {
    w = monitor->x + monitor->width - x;
  }
  if (y + h > monitor->y + monitor->height) {
    h = monitor->y + monitor->height - y;
  }
  Rect new_rect = {.x = x, .y = y, .w = w, .h = h};

  if (i < num_windows) {
    // Move the existing window.
    ClearWindowUncoveredAreas(i, new_rect);
    XMoveResizeWindow(display, windows[i], x, y, w, h);
    window_rects[i] = new_rect;
    return 1;
  }

  if (i > num_windows) {
    // Need to add windows in ]num_windows..i[ first.
    Log("Unreachable code - can't create monitor sequences with holes");
    abort();
  }

  // Add a new window.
  XSetWindowAttributes attrs = {0};
  attrs.background_pixel = xcolor_background.pixel;
  if (i == MAIN_WINDOW) {
    // Reuse the main_window (so this window gets protected from overlap by
    // main).
    XMoveResizeWindow(display, main_window, x, y, w, h);
    XChangeWindowAttributes(display, main_window, CWBackPixel, &attrs);
    windows[i] = main_window;
  } else {
    // Create a new window.
    windows[i] =
        XCreateWindow(display, parent_window, x, y, w, h, 0, CopyFromParent,
                      InputOutput, CopyFromParent, CWBackPixel, &attrs);
    if (windows[i] == None) {
      Log("XCreateWindow failed");
      return 0;
    }
    SetWMProperties(display, windows[i], "xsecurelock", "auth_x11_screen", argc,
                    argv);
    // We should always make sure that main_window stays on top of all others.
    // I.e. our auth sub-windows shall between "sandwiched" between auth and
    // saver window. That way, main.c's protections of the auth window can stay
    // effective.
    Window stacking_order[2];
    stacking_order[0] = main_window;
    stacking_order[1] = windows[i];
    XRestackWindows(display, stacking_order, 2);
  }

  // Create its data structures.
  XGCValues gcattrs;
  gcattrs.function = GXcopy;
  gcattrs.foreground = xcolor_foreground.pixel;
  gcattrs.background = xcolor_background.pixel;
  if (core_font != NULL) {
    gcattrs.font = core_font->fid;
  }
  gcs[i] = XCreateGC(display, windows[i],
                     GCFunction | GCForeground | GCBackground |
                         (core_font != NULL ? GCFont : 0),
                     &gcattrs);
  if (gcs[i] == NULL) {
    Log("XCreateGC failed");
    CleanupPerMonitorWindow(i);
    return 0;
  }
  gcattrs.foreground = xcolor_warning.pixel;
  gcs_warning[i] = XCreateGC(display, windows[i],
                             GCFunction | GCForeground | GCBackground |
                                 (core_font != NULL ? GCFont : 0),
                             &gcattrs);
  if (gcs_warning[i] == NULL) {
    Log("XCreateGC failed for warning GC");
    CleanupPerMonitorWindow(i);
    return 0;
  }
#ifdef HAVE_XFT_EXT
  xft_draws[i] = XftDrawCreate(
      display, windows[i], DefaultVisual(display, DefaultScreen(display)),
      DefaultColormap(display, DefaultScreen(display)));
  if (xft_font != NULL && xft_draws[i] == NULL) {
    Log("XftDrawCreate failed");
    CleanupPerMonitorWindow(i);
    return 0;
  }
#endif

  // This window is now ready to use.
  XMapWindow(display, windows[i]);
  window_rects[i] = new_rect;
  num_windows = i + 1;
  return 1;
}

static int UpdatePerMonitorWindows(int monitors_changed, int region_w,
                                   int region_h, int x_offset, int y_offset) {
  static size_t num_monitors = 0;
  static Monitor monitors[MAX_WINDOWS];

  if (monitors_changed) {
    num_monitors = GetMonitors(display, parent_window, monitors, MAX_WINDOWS);
  }

  if (single_auth_window) {
    Window unused_root, unused_child;
    int unused_root_x, unused_root_y, x, y;
    unsigned int unused_mask;
    XQueryPointer(display, parent_window, &unused_root, &unused_child,
                  &unused_root_x, &unused_root_y, &x, &y, &unused_mask);
    for (size_t i = 0; i < num_monitors; ++i) {
      if (x >= monitors[i].x && x < monitors[i].x + monitors[i].width &&
          y >= monitors[i].y && y < monitors[i].y + monitors[i].height) {
        if (!CreateOrUpdatePerMonitorWindow(0, &monitors[i], region_w, region_h,
                                           x_offset, y_offset)) {
          DestroyPerMonitorWindows(0);
          return 0;
        }
        return 1;
      }
    }
    if (num_monitors > 0) {
      if (!CreateOrUpdatePerMonitorWindow(0, &monitors[0], region_w, region_h,
                                          x_offset, y_offset)) {
        DestroyPerMonitorWindows(0);
        return 0;
      }
      DestroyPerMonitorWindows(1);
    } else {
      DestroyPerMonitorWindows(0);
    }
    return 1;
  }

  // 1 window per monitor.
  size_t new_num_windows = num_monitors;

  // Update or create everything.
  for (size_t i = 0; i < new_num_windows; ++i) {
    if (!CreateOrUpdatePerMonitorWindow(i, &monitors[i], region_w, region_h,
                                        x_offset, y_offset)) {
      DestroyPerMonitorWindows(0);
      return 0;
    }
  }

  // Kill all the old stuff.
  DestroyPerMonitorWindows(new_num_windows);

  if (num_windows != new_num_windows) {
    Log("Unreachable code - expected to get %d windows, got %d",
        (int)new_num_windows, (int)num_windows);
  }
  return 1;
}

static int TextAscent(void) {
#ifdef HAVE_XFT_EXT
  if (xft_font != NULL) {
    return xft_font->ascent;
  }
#endif
  return core_font->max_bounds.ascent;
}

static int TextDescent(void) {
#ifdef HAVE_XFT_EXT
  if (xft_font != NULL) {
    return xft_font->descent;
  }
#endif
  return core_font->max_bounds.descent;
}

#ifdef HAVE_XFT_EXT
// Returns the amount of pixels to expand the logical box in extents so it
// covers the visible box.
static int XGlyphInfoExpandAmount(XGlyphInfo *extents) {
  // Use whichever is larger - visible bounding box (bigger if font is italic)
  // or spacing to next character (bigger if last character is a space).
  // Best reference I could find:
  //   https://keithp.com/~keithp/render/Xft.tutorial
  // Visible bounding box: [-x, -x + width[
  // Logical bounding box: [0, xOff[
  // For centering we should always use the logical bounding box, however for
  // erasing we should use the visible bounding box. Thus our goal is to
  // expand the _logical_ box to fully cover the _visible_ box:
  int expand_left = extents->x;
  int expand_right = -extents->x + extents->width - extents->xOff;
  int expand_max = expand_left > expand_right ? expand_left : expand_right;
  int expand_positive = expand_max > 0 ? expand_max : 0;
  return expand_positive;
}
#endif

static int TextWidth(const char *string, int len) {
#ifdef HAVE_XFT_EXT
  if (xft_font != NULL) {
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, xft_font, (const FcChar8 *)string, len,
                       &extents);
    return extents.xOff + 2 * XGlyphInfoExpandAmount(&extents);
  }
#endif
  return XTextWidth(core_font, string, len);
}

static void DrawString(int monitor, int x, int y, int is_warning,
                       const char *string, int len) {
#ifdef HAVE_XFT_EXT
  if (xft_font != NULL) {
    // HACK: Query text extents here to make the text fit into the specified
    // box. For y this is covered by the usual ascent/descent behavior - for x
    // we however do have to work around font descents being drawn to the left
    // of the cursor.
    XGlyphInfo extents;
    XftTextExtentsUtf8(display, xft_font, (const FcChar8 *)string, len,
                       &extents);
    XftDrawStringUtf8(xft_draws[monitor],
                      is_warning ? &xft_color_warning : &xft_color_foreground,
                      xft_font, x + XGlyphInfoExpandAmount(&extents), y,
                      (const FcChar8 *)string, len);
    return;
  }
#endif
  XDrawString(display, windows[monitor],
              is_warning ? gcs_warning[monitor] : gcs[monitor], x, y, string,
              len);
}

static int AuthDialogInset(void) { return auth_padding + auth_border_size; }

static void DrawDialogBorder(size_t window_index, int region_w, int region_h) {
  if (auth_border_size <= 0) {
    return;
  }

  XSetLineAttributes(display, gcs[window_index], auth_border_size, LineSolid,
                     CapButt, JoinMiter);
  int border_offset = auth_border_size / 2;
  XDrawRectangle(display, windows[window_index], gcs[window_index],
                 border_offset, border_offset, region_w - auth_border_size - 1,
                 region_h - auth_border_size - 1);
}

static void StrAppend(char **output, size_t *output_size, const char *input,
                      size_t input_size) {
  if (*output_size <= input_size) {
    // Cut the input off. Sorry.
    input_size = *output_size - 1;
  }
  memcpy(*output, input, input_size);
  *output += input_size;
  *output_size -= input_size;
}

static void BuildTitle(char *output, size_t output_size, const char *input) {
  if (show_username) {
    size_t username_len = strlen(username);
    StrAppend(&output, &output_size, username, username_len);
  }

  if (show_username && show_hostname) {
    StrAppend(&output, &output_size, "@", 1);
  }

  if (show_hostname) {
    size_t hostname_len =
        show_hostname > 1 ? strlen(hostname) : strcspn(hostname, ".");
    StrAppend(&output, &output_size, hostname, hostname_len);
  }

  if (*input == 0) {
    *output = 0;
    return;
  }

  if (show_username || show_hostname) {
    StrAppend(&output, &output_size, " - ", 3);
  }
  strncpy(output, input, output_size - 1);
  output[output_size - 1] = 0;
}

/*! \brief Display a string in the window.
 *
 * The given title and message will be displayed on all screens. In case caps
 * lock is enabled, the string's case will be inverted.
 *
 * \param title The title of the message.
 * \param str The message itself.
 * \param is_warning Whether to use the warning style to display the message.
 */
static int DisplayMessage(const char *title, const char *str, int is_warning) {
  char full_title[256];
  BuildTitle(full_title, sizeof(full_title), title);

  int th = TextAscent() + TextDescent() + LINE_SPACING;
  int to = TextAscent() + LINE_SPACING / 2;  // Text at to fits into 0 to th.

  int len_full_title = strlen(full_title);
  int tw_full_title = TextWidth(full_title, len_full_title);

  int len_str = strlen(str);
  int tw_str = TextWidth(str, len_str);

  int indicators_warning = 0;
  int have_multiple_layouts = 0;
  const char *indicators =
      GetIndicators(&indicators_warning, &have_multiple_layouts);
  int len_indicators = strlen(indicators);
  int tw_indicators = TextWidth(indicators, len_indicators);

  const char *switch_layout =
      have_multiple_layouts ? "Press Ctrl-Tab to switch keyboard layout" : "";
  int len_switch_layout = strlen(switch_layout);
  int tw_switch_layout = TextWidth(switch_layout, len_switch_layout);

  const char *switch_user = have_switch_user_command
                                ? "Press Ctrl-Alt-O or Win-O to switch user"
                                : "";
  int len_switch_user = strlen(switch_user);
  int tw_switch_user = TextWidth(switch_user, len_switch_user);

  char datetime[80] = "";
  if (show_datetime) {
    time_t rawtime;
    struct tm timeinfo_buf;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime_r(&rawtime, &timeinfo_buf);
    if (timeinfo == NULL ||
        strftime(datetime, sizeof(datetime), datetime_format, timeinfo) == 0) {
      // The datetime buffer was too small to fit the time format, and in this
      // case the buffer contents are undefined. Let's just make it a valid
      // empty string then so all else will go well.
      datetime[0] = 0;
    }
  }

  int len_datetime = strlen(datetime);
  int tw_datetime = TextWidth(datetime, len_datetime);

  // Compute the region we will be using, relative to cx and cy.
  int box_w = tw_full_title;
  if (box_w < tw_datetime) {
    box_w = tw_datetime;
  }
  if (box_w < tw_str) {
    box_w = tw_str;
  }
  if (box_w < tw_indicators) {
    box_w = tw_indicators;
  }
  if (box_w < tw_switch_layout) {
    box_w = tw_switch_layout;
  }
  if (box_w < tw_switch_user) {
    box_w = tw_switch_user;
  }
  int box_h = (4 + have_multiple_layouts + have_switch_user_command +
               show_datetime * 2) *
              th;
  int region_inset = AuthDialogInset();
  int region_w = box_w + 2 * region_inset;
  int region_h = box_h + 2 * region_inset;

  if (burnin_mitigation_max_offset_change > 0) {
    x_offset = StepBurnInOffset(&prompt_rng, x_offset,
                                burnin_mitigation_max_offset,
                                burnin_mitigation_max_offset_change);
    y_offset = StepBurnInOffset(&prompt_rng, y_offset,
                                burnin_mitigation_max_offset,
                                burnin_mitigation_max_offset_change);
  }

  if (!UpdatePerMonitorWindows(per_monitor_windows_dirty, region_w, region_h,
                               x_offset, y_offset)) {
    per_monitor_windows_dirty = 1;
    return 0;
  }
  per_monitor_windows_dirty = 0;

  for (size_t i = 0; i < num_windows; ++i) {
    int cx = region_w / 2;
    int cy = region_h / 2;
    int y = cy + to - box_h / 2;

    XClearWindow(display, windows[i]);
    DrawDialogBorder(i, region_w, region_h);

    if (show_datetime) {
      DrawString(i, cx - tw_datetime / 2, y, 0, datetime, len_datetime);
      y += th * 2;
    }

    DrawString(i, cx - tw_full_title / 2, y, is_warning, full_title,
               len_full_title);
    y += th * 2;

    DrawString(i, cx - tw_str / 2, y, is_warning, str, len_str);
    y += th;

    DrawString(i, cx - tw_indicators / 2, y, indicators_warning, indicators,
               len_indicators);
    y += th;

    if (have_multiple_layouts) {
      DrawString(i, cx - tw_switch_layout / 2, y, 0, switch_layout,
                 len_switch_layout);
      y += th;
    }

    if (have_switch_user_command) {
      DrawString(i, cx - tw_switch_user / 2, y, 0, switch_user,
                 len_switch_user);
      // y += th;
    }
  }

  // Make the things just drawn appear on the screen as soon as possible.
  XFlush(display);
  return 1;
}

static void WaitForKeypress(int seconds) {
  // Sleep for up to 1 second _or_ a key press.
  struct pollfd pfd;
  pfd.fd = 0;
  pfd.events = POLLIN | POLLHUP;
  pfd.revents = 0;
  (void)RetryPoll(&pfd, 1, seconds * 1000);
}

/*! \brief Bump the position for the password "cursor".
 *
 * If pwlen > 0:
 * Precondition: pos in 0..PARANOID_PASSWORD_LENGTH-1.
 * Postcondition: pos' in 1..PARANOID_PASSWORD_LENGTH-1.
 * Postcondition: abs(pos' - pos) >= PARANOID_PASSWORD_MIN_CHANGE.
 * Postcondition: pos' is uniformly distributed among all permitted choices.
 * If pwlen == 0:
 * Postcondition: pos' is 0.
 *
 * \param pwlen The current password length.
 * \param pos The initial cursor position; will get updated.
 * \param last_keystroke The time of last keystroke; will get updated.
 */
static void BumpDisplayMarker(size_t pwlen, size_t *pos,
                              struct timeval *last_keystroke) {
  gettimeofday(last_keystroke, NULL);

  // Empty password: always put at 0.
  if (pwlen == 0) {
    *pos = 0;
    return;
  }

  *pos = NextDisplayMarker(&prompt_rng, *pos, PARANOID_PASSWORD_LENGTH,
                           PARANOID_PASSWORD_MIN_CHANGE);
}

//! The size of the buffer to store the password in. Not NUL terminated.
#define PWBUF_SIZE 256

//! The size of the buffer to use for display, with space for cursor and NUL.
#define DISPLAYBUF_SIZE (PWBUF_SIZE + 2)

static void ShowFromArray(const char *const *array, size_t displaymarker,
                          char *displaybuf, size_t displaybufsize,
                          size_t *displaylen) {
  const char *selection = array[displaymarker];
  size_t selection_len = strlen(selection);
  size_t copy_len = selection_len;
  if (copy_len >= displaybufsize) {
    copy_len = displaybufsize - 1;
  }
  memcpy(displaybuf, selection, copy_len);
  displaybuf[copy_len] = 0;
  *displaylen = selection_len;
}

enum PromptResult {
  PROMPT_RESULT_SUBMITTED,
  PROMPT_RESULT_CANCELLED,
  PROMPT_RESULT_FAILED,
};

enum AuthActivityResult {
  AUTH_ACTIVITY_REDRAW,
  AUTH_ACTIVITY_INPUT_READY,
  AUTH_ACTIVITY_EXTRA_FD_READY,
  AUTH_ACTIVITY_TIMEOUT,
  AUTH_ACTIVITY_FAILED,
};

enum StaticMessageResult {
  STATIC_MESSAGE_RESULT_ADVANCE,
  STATIC_MESSAGE_RESULT_CANCELLED,
  STATIC_MESSAGE_RESULT_FAILED,
};

static void DrainMonitorChangeEvents(void) {
  XEvent ev;
  while (XPending(display) && (XNextEvent(display, &ev), 1)) {
    if (IsMonitorChangeEvent(display, ev.type)) {
      per_monitor_windows_dirty = 1;
    }
  }
}

static enum AuthActivityResult WaitForAuthActivity(int extra_read_fd,
                                                   time_t *deadline,
                                                   int poll_only,
                                                   char *inputbuf) {
  int timeout_ms;
  if (poll_only) {
    timeout_ms = 0;
  } else {
    timeout_ms = BLINK_INTERVAL / 1000;
  }

  struct pollfd fds[2];
  nfds_t nfds = 1;
  fds[0].fd = 0;
  fds[0].events = POLLIN | POLLHUP;
  fds[0].revents = 0;
  if (extra_read_fd >= 0) {
    fds[1].fd = extra_read_fd;
    fds[1].events = POLLIN | POLLHUP;
    fds[1].revents = 0;
    nfds = 2;
  }

  int ready = RetryPoll(fds, nfds, timeout_ms);
  if (ready < 0) {
    LogErrno("poll");
    return AUTH_ACTIVITY_FAILED;
  }

  time_t now = time(NULL);
  if (now > *deadline) {
    return AUTH_ACTIVITY_TIMEOUT;
  }
  if (*deadline > now + prompt_timeout) {
    // Guard against the system clock stepping back.
    *deadline = now + prompt_timeout;
  }
  if (ready == 0) {
    return AUTH_ACTIVITY_REDRAW;
  }
  if (extra_read_fd >= 0 && (fds[1].revents & POLLNVAL)) {
    Log("poll: invalid extra fd %d", extra_read_fd);
    return AUTH_ACTIVITY_FAILED;
  }
  if (fds[0].revents & POLLNVAL) {
    Log("poll: invalid stdin fd");
    return AUTH_ACTIVITY_FAILED;
  }
  if (extra_read_fd >= 0 && (fds[1].revents & (POLLIN | POLLHUP))) {
    return AUTH_ACTIVITY_EXTRA_FD_READY;
  }
  if ((fds[0].revents & (POLLIN | POLLHUP)) == 0) {
    Log("poll: unexpected auth activity event mask %#x", fds[0].revents);
    return AUTH_ACTIVITY_FAILED;
  }

  ssize_t nread = RetryRead(0, inputbuf, 1);
  if (nread < 0) {
    LogErrno("read");
    return AUTH_ACTIVITY_FAILED;
  }
  if (nread == 0) {
    Log("EOF on password input - bailing out");
    return AUTH_ACTIVITY_FAILED;
  }

  *deadline = now + prompt_timeout;
  return AUTH_ACTIVITY_INPUT_READY;
}

static enum StaticMessageResult WaitStaticMessage(const char *title,
                                                  const char *message,
                                                  int is_warning,
                                                  int extra_read_fd) {
  int played_sound = 0;
  char inputbuf = 0;
  time_t deadline = time(NULL) + prompt_timeout;

  for (;;) {
    if (!DisplayMessage(title, message, is_warning)) {
      return STATIC_MESSAGE_RESULT_FAILED;
    }
    if (!played_sound) {
      PlaySound(is_warning ? SOUND_ERROR : SOUND_INFO);
      played_sound = 1;
    }

    switch (WaitForAuthActivity(extra_read_fd, &deadline, 0, &inputbuf)) {
      case AUTH_ACTIVITY_REDRAW:
        DrainMonitorChangeEvents();
        break;
      case AUTH_ACTIVITY_INPUT_READY:
        DrainMonitorChangeEvents();
        if (inputbuf == '\033') {
          return STATIC_MESSAGE_RESULT_CANCELLED;
        }
        break;
      case AUTH_ACTIVITY_EXTRA_FD_READY:
        DrainMonitorChangeEvents();
        return STATIC_MESSAGE_RESULT_ADVANCE;
      case AUTH_ACTIVITY_TIMEOUT:
        Log("AUTH_TIMEOUT hit");
        return STATIC_MESSAGE_RESULT_CANCELLED;
      case AUTH_ACTIVITY_FAILED:
        return STATIC_MESSAGE_RESULT_FAILED;
    }
  }
}

static void ClearFreeString(char **message) {
  if (*message == NULL) {
    return;
  }
  explicit_bzero(*message, strlen(*message));
  free(*message);
  *message = NULL;
}

static void TerminateAuthproto(pid_t childpid) {
  if (kill(childpid, SIGTERM) != 0 && errno != ESRCH) {
    LogErrno("kill");
  }
}

/*! \brief Ask a question to the user.
 *
 * \param msg The message.
 * \param response The response will be stored in a newly allocated buffer here.
 *   The caller is supposed to eventually free() it.
 * \param echo If true, the input will be shown; otherwise it will be hidden
 *   (password entry).
 * \return Whether the prompt was submitted, cancelled, or failed locally.
 */
static enum PromptResult Prompt(const char *msg, char **response, int echo) {
  // Ask something. Return strdup'd string.
  struct {
    // The received X11 event.
    XEvent ev;

    // Input buffer. Not NUL-terminated.
    char pwbuf[PWBUF_SIZE];
    // Current input length.
    size_t pwlen;

    // Display buffer. If echo is 0, this will only contain asterisks, a
    // possible cursor, and be NUL-terminated.
    char displaybuf[DISPLAYBUF_SIZE];
    // Display buffer length.
    size_t displaylen;

    // The display marker changes on every input action to a value from 0 to
    // PARANOID_PASSWORD-1. It indicates where to display the "cursor".
    size_t displaymarker;

    // Character read buffer.
    char inputbuf;

    // The time of last keystroke.
    struct timeval last_keystroke;

    // Temporary position variables that might leak properties about the
    // password and thus are in the private struct too.
    size_t prevpos;
    size_t pos;
    int len;
  } priv;
  int blink_state = 0;

  if (!echo && MLOCK_PAGE(&priv, sizeof(priv)) < 0) {
    LogErrno("mlock");
    // We continue anyway, as the user being unable to unlock the screen is
    // worse. But let's alert the user.
    if (!DisplayMessage("Error", "Password will not be stored securely.", 1)) {
      return PROMPT_RESULT_FAILED;
    }
    WaitForKeypress(1);
  }

  priv.pwlen = 0;
  priv.displaymarker = 0;

  time_t deadline = time(NULL) + prompt_timeout;

  // Unfortunately we may have to break out of multiple loops at once here but
  // still do common cleanup work. So we have to track the return value in a
  // variable.
  enum PromptResult result = PROMPT_RESULT_CANCELLED;
  int done = 0;
  int played_sound = 0;

  while (!done) {
    if (echo) {
      if (priv.pwlen != 0) {
        memcpy(priv.displaybuf, priv.pwbuf, priv.pwlen);
      }
      priv.displaylen = priv.pwlen;
      // Note that priv.pwlen <= sizeof(priv.pwbuf) and thus
      // priv.pwlen + 2 <= sizeof(priv.displaybuf).
      priv.displaybuf[priv.displaylen] = blink_state ? ' ' : *cursor;
      priv.displaybuf[priv.displaylen + 1] = '\0';
    } else {
      switch (password_prompt) {
        case PASSWORD_PROMPT_ASTERISKS: {
          mblen(NULL, 0);
          priv.pos = priv.displaylen = 0;
          while (priv.pos < priv.pwlen) {
            ++priv.displaylen;
            // Note: this won't read past priv.pwlen.
            priv.len = mblen(priv.pwbuf + priv.pos, priv.pwlen - priv.pos);
            if (priv.len <= 0) {
              // This guarantees to "eat" one byte each step. Therefore,
              // priv.displaylen <= priv.pwlen is ensured.
              break;
            }
            priv.pos += priv.len;
          }
          memset(priv.displaybuf, '*', priv.displaylen);
          // Note that priv.pwlen <= sizeof(priv.pwbuf) and thus
          // priv.pwlen + 2 <= sizeof(priv.displaybuf).
          priv.displaybuf[priv.displaylen] = blink_state ? ' ' : *cursor;
          priv.displaybuf[priv.displaylen + 1] = '\0';
          break;
        }

        case PASSWORD_PROMPT_HIDDEN: {
          priv.displaylen = 0;
          priv.displaybuf[0] = '\0';
          break;
        }

        case PASSWORD_PROMPT_DISCO: {
          if (FormatDiscoPrompt(priv.displaymarker, priv.displaybuf,
                                sizeof(priv.displaybuf),
                                &priv.displaylen) != 0) {
            Log("Disco prompt rendering overflow; falling back to cursor");
            priv.displaylen = PARANOID_PASSWORD_LENGTH;
            memset(priv.displaybuf, '_', priv.displaylen);
            priv.displaybuf[priv.displaymarker] = blink_state ? '-' : '|';
            priv.displaybuf[priv.displaylen] = '\0';
          }
          break;
        }

        case PASSWORD_PROMPT_EMOJI: {
          ShowFromArray(emoji, priv.displaymarker, priv.displaybuf,
                        sizeof(priv.displaybuf), &priv.displaylen);
          break;
        }

        case PASSWORD_PROMPT_EMOTICON: {
          ShowFromArray(emoticons, priv.displaymarker, priv.displaybuf,
                        sizeof(priv.displaybuf), &priv.displaylen);
          break;
        }

        case PASSWORD_PROMPT_KAOMOJI: {
          ShowFromArray(kaomoji, priv.displaymarker, priv.displaybuf,
                        sizeof(priv.displaybuf), &priv.displaylen);
          break;
        }

#if __STDC_VERSION__ >= 199901L
        case PASSWORD_PROMPT_TIME:
        case PASSWORD_PROMPT_TIME_HEX: {
          if (priv.pwlen == 0) {
            strncpy(priv.displaybuf, "----", DISPLAYBUF_SIZE - 1);
            priv.displaybuf[DISPLAYBUF_SIZE - 1] = 0;
          } else {
            if (password_prompt == PASSWORD_PROMPT_TIME) {
              snprintf(priv.displaybuf, DISPLAYBUF_SIZE,
                       "%" PRId64 ".%06" PRId64,
                       (int64_t)priv.last_keystroke.tv_sec,
                       (int64_t)priv.last_keystroke.tv_usec);
            } else {
              snprintf(priv.displaybuf, DISPLAYBUF_SIZE, "%#" PRIx64,
                       (int64_t)priv.last_keystroke.tv_sec * 1000000 +
                           (int64_t)priv.last_keystroke.tv_usec);
            }
            priv.displaybuf[DISPLAYBUF_SIZE - 1] = 0;
          }
          break;
        }
#endif

        default:
        case PASSWORD_PROMPT_CURSOR: {
          priv.displaylen = PARANOID_PASSWORD_LENGTH;
          memset(priv.displaybuf, '_', priv.displaylen);
          priv.displaybuf[priv.displaymarker] = blink_state ? '-' : '|';
          priv.displaybuf[priv.displaylen] = '\0';
          break;
        }
      }
    }
    if (!DisplayMessage(msg, priv.displaybuf, 0)) {
      result = PROMPT_RESULT_FAILED;
      done = 1;
      break;
    }

    if (!played_sound) {
      PlaySound(SOUND_PROMPT);
      played_sound = 1;
    }

    // Blink the cursor.
    if (auth_cursor_blink) {
      blink_state = !blink_state;
    }

    int poll_only = 0;

    while (!done) {
      switch (WaitForAuthActivity(-1, &deadline, poll_only, &priv.inputbuf)) {
        case AUTH_ACTIVITY_REDRAW:
          goto redraw;
        case AUTH_ACTIVITY_TIMEOUT:
          Log("AUTH_TIMEOUT hit");
          result = PROMPT_RESULT_CANCELLED;
          done = 1;
          break;
        case AUTH_ACTIVITY_FAILED:
          result = PROMPT_RESULT_FAILED;
          done = 1;
          break;
        case AUTH_ACTIVITY_EXTRA_FD_READY:
          Log("Unexpected authproto readiness while prompting");
          result = PROMPT_RESULT_FAILED;
          done = 1;
          break;
        case AUTH_ACTIVITY_INPUT_READY:
          poll_only = 1;
          // Force the cursor to be in visible state while typing.
          blink_state = 0;
          break;
      }
      if (done) {
        break;
      }
      switch (priv.inputbuf) {
        case '\b':      // Backspace.
        case '\177': {  // Delete (note: i3lock does not handle this one).
          // Backwards skip with multibyte support.
          mblen(NULL, 0);
          priv.pos = priv.prevpos = 0;
          while (priv.pos < priv.pwlen) {
            priv.prevpos = priv.pos;
            // Note: this won't read past priv.pwlen.
            priv.len = mblen(priv.pwbuf + priv.pos, priv.pwlen - priv.pos);
            if (priv.len <= 0) {
              // This guarantees to "eat" one byte each step. Therefore,
              // this cannot loop endlessly.
              break;
            }
            priv.pos += priv.len;
          }
          priv.pwlen = priv.prevpos;
          BumpDisplayMarker(priv.pwlen, &priv.displaymarker,
                            &priv.last_keystroke);
          break;
        }
        case '\001':  // Ctrl-A.
          // Clearing input line on just Ctrl-A is odd - but commonly
          // requested. In most toolkits, Ctrl-A does not immediately erase but
          // almost every keypress other than arrow keys will erase afterwards.
          priv.pwlen = 0;
          BumpDisplayMarker(priv.pwlen, &priv.displaymarker,
                            &priv.last_keystroke);
          break;
        case '\023':  // Ctrl-S.
          SwitchKeyboardLayout();
          break;
        case '\025':  // Ctrl-U.
          // Delete the entire input line.
          // i3lock: supports Ctrl-U but not Ctrl-A.
          // xscreensaver: supports Ctrl-U and Ctrl-X but not Ctrl-A.
          priv.pwlen = 0;
          BumpDisplayMarker(priv.pwlen, &priv.displaymarker,
                            &priv.last_keystroke);
          break;
        case 0:  // Shouldn't happen.
          result = PROMPT_RESULT_FAILED;
          done = 1;
          break;
        case '\033':  // Escape.
          result = PROMPT_RESULT_CANCELLED;
          done = 1;
          break;
        case '\r':  // Return.
        case '\n':  // Return.
          *response = malloc(priv.pwlen + 1);
          if (*response == NULL) {
            LogErrno("malloc");
            result = PROMPT_RESULT_FAILED;
            done = 1;
            break;
          }
          if (!echo && MLOCK_PAGE(*response, priv.pwlen + 1) < 0) {
            LogErrno("mlock");
            // We continue anyway, as the user being unable to unlock the screen
            // is worse. But let's alert the user of this.
            if (!DisplayMessage("Error",
                                "Password has not been stored securely.", 1)) {
              result = PROMPT_RESULT_FAILED;
              done = 1;
              break;
            }
            WaitForKeypress(1);
          }
          if (priv.pwlen != 0) {
            memcpy(*response, priv.pwbuf, priv.pwlen);
          }
          (*response)[priv.pwlen] = 0;
          result = PROMPT_RESULT_SUBMITTED;
          done = 1;
          break;
        default:
          if (priv.inputbuf >= '\000' && priv.inputbuf <= '\037') {
            // Other control character. We ignore them (and specifically do not
            // update the cursor on them) to "discourage" their use in
            // passwords, as most login screens do not support them anyway.
            break;
          }
          if (priv.pwlen < sizeof(priv.pwbuf)) {
            priv.pwbuf[priv.pwlen] = priv.inputbuf;
            ++priv.pwlen;
            BumpDisplayMarker(priv.pwlen, &priv.displaymarker,
                              &priv.last_keystroke);
          } else {
            Log("Password entered is too long - bailing out");
            result = PROMPT_RESULT_FAILED;
            done = 1;
            break;
          }
          break;
      }
    }

redraw:
    if (!done) {
      DrainMonitorChangeEvents();
    }
  }

  // priv contains password related data, so better clear it.
  explicit_bzero(&priv, sizeof(priv));

  if (!done) {
    Log("Unreachable code - the loop above must set done");
  }
  return result;
}

/*! \brief Perform authentication using a helper proxy.
 *
 * \return The authentication status (0 for OK, 1 otherwise).
 */
static int Authenticate(void) {
  int requestfd[2], responsefd[2];
  if (PipeCloexec(requestfd) != 0) {
    LogErrno("PipeCloexec");
    return 1;
  }
  if (PipeCloexec(responsefd) != 0) {
    int saved_errno = errno;
    close(requestfd[0]);
    close(requestfd[1]);
    errno = saved_errno;
    LogErrno("PipeCloexec");
    return 1;
  }

  // Use authproto_pam.
  pid_t childpid = ForkWithoutSigHandlers();
  if (childpid == -1) {
    LogErrno("fork");
    return 1;
  }

  if (childpid == 0) {
    // Child process. Just run authproto_pam.
    // But first, move requestfd[1] to 1 and responsefd[0] to 0.
    close(requestfd[0]);
    close(responsefd[1]);

    if (requestfd[1] == 0) {
      // Tricky case. We don't _expect_ this to happen - after all,
      // initially our own fd 0 should be bound to xsecurelock's main
      // program - but nevertheless let's handle it.
      // At least this implies that no other fd is 0.
      int requestfd1 = dup(requestfd[1]);
      if (requestfd1 == -1) {
        LogErrno("dup");
        _exit(EXIT_FAILURE);
      }
      close(requestfd[1]);
      if (dup2(responsefd[0], 0) == -1) {
        LogErrno("dup2");
        _exit(EXIT_FAILURE);
      }
      close(responsefd[0]);
      if (requestfd1 != 1) {
        if (dup2(requestfd1, 1) == -1) {
          LogErrno("dup2");
          _exit(EXIT_FAILURE);
        }
        close(requestfd1);
      }
    } else {
      if (responsefd[0] != 0) {
        if (dup2(responsefd[0], 0) == -1) {
          LogErrno("dup2");
          _exit(EXIT_FAILURE);
        }
        close(responsefd[0]);
      }
      if (requestfd[1] != 1) {
        if (dup2(requestfd[1], 1) == -1) {
          LogErrno("dup2");
          _exit(EXIT_FAILURE);
        }
        close(requestfd[1]);
      }
    }
    {
      const char *args[2] = {authproto_executable, NULL};
      ExecvHelper(authproto_executable, args);
      sleep(2);  // Reduce log spam or other effects from failed execv.
      _exit(EXIT_FAILURE);
    }
  }

  // Otherwise, we're in the parent process.
  close(requestfd[1]);
  close(responsefd[0]);
#define SHOW_PROCESSING_OR_FAIL()        \
  do {                                   \
    if (!DisplayMessage("Processing...", "", 0)) { \
      ClearFreeString(&message);         \
      goto done;                         \
    }                                    \
  } while (0)
  for (;;) {
    char *message;
    char *response;
    char type = ReadPacket(requestfd[0], &message, 1);
    switch (type) {
      case PTYPE_INFO_MESSAGE:
        switch (WaitStaticMessage("PAM says", message, 0, requestfd[0])) {
          case STATIC_MESSAGE_RESULT_ADVANCE:
            break;
          case STATIC_MESSAGE_RESULT_CANCELLED:
            TerminateAuthproto(childpid);
            ClearFreeString(&message);
            goto done;
          case STATIC_MESSAGE_RESULT_FAILED:
            ClearFreeString(&message);
            goto done;
        }
        ClearFreeString(&message);
        break;
      case PTYPE_ERROR_MESSAGE:
        switch (WaitStaticMessage("Error", message, 1, requestfd[0])) {
          case STATIC_MESSAGE_RESULT_ADVANCE:
            break;
          case STATIC_MESSAGE_RESULT_CANCELLED:
            TerminateAuthproto(childpid);
            ClearFreeString(&message);
            goto done;
          case STATIC_MESSAGE_RESULT_FAILED:
            ClearFreeString(&message);
            goto done;
        }
        ClearFreeString(&message);
        break;
      case PTYPE_PROMPT_LIKE_USERNAME:
        switch (Prompt(message, &response, 1)) {
          case PROMPT_RESULT_SUBMITTED:
            WritePacket(responsefd[1], PTYPE_RESPONSE_LIKE_USERNAME, response);
            explicit_bzero(response, strlen(response));
            free(response);
            SHOW_PROCESSING_OR_FAIL();
            break;
          case PROMPT_RESULT_CANCELLED:
            WritePacket(responsefd[1], PTYPE_RESPONSE_CANCELLED, "");
            SHOW_PROCESSING_OR_FAIL();
            break;
          case PROMPT_RESULT_FAILED:
            ClearFreeString(&message);
            goto done;
        }
        ClearFreeString(&message);
        break;
      case PTYPE_PROMPT_LIKE_PASSWORD:
        switch (Prompt(message, &response, 0)) {
          case PROMPT_RESULT_SUBMITTED:
            WritePacket(responsefd[1], PTYPE_RESPONSE_LIKE_PASSWORD, response);
            explicit_bzero(response, strlen(response));
            free(response);
            SHOW_PROCESSING_OR_FAIL();
            break;
          case PROMPT_RESULT_CANCELLED:
            WritePacket(responsefd[1], PTYPE_RESPONSE_CANCELLED, "");
            SHOW_PROCESSING_OR_FAIL();
            break;
          case PROMPT_RESULT_FAILED:
            ClearFreeString(&message);
            goto done;
        }
        ClearFreeString(&message);
        break;
      case 0:
        goto done;
      default:
        Log("Unknown message type %02x", (int)type);
        ClearFreeString(&message);
        goto done;
    }
  }
#undef SHOW_PROCESSING_OR_FAIL
done:
  close(requestfd[0]);
  close(responsefd[1]);
  int status;
  if (!WaitProc("authproto", &childpid, 1, 0, &status)) {
    Log("WaitPgrp returned false but we were blocking");
    abort();
  }
  if (status == 0) {
    PlaySound(SOUND_SUCCESS);
  }
  return status != 0;
}

static enum PasswordPrompt GetPasswordPromptFromFlags(
    int paranoid_password_flag, const char *password_prompt_flag) {
  if (!*password_prompt_flag) {
    return paranoid_password_flag ? PASSWORD_PROMPT_CURSOR
                                  : PASSWORD_PROMPT_ASTERISKS;
  }

  for (enum PasswordPrompt prompt = 0; prompt < PASSWORD_PROMPT_COUNT;
       ++prompt) {
    if (strcmp(password_prompt_flag, PasswordPromptStrings[prompt]) == 0) {
      return prompt;
    }
  }

  Log("Invalid XSECURELOCK_PASSWORD_PROMPT value; defaulting to cursor");
  return PASSWORD_PROMPT_CURSOR;
}

#ifdef HAVE_XFT_EXT
static XftFont *FixedXftFontOpenName(Display *display, int screen,
                                     const char *font_name) {
  XftFont *xft_font = XftFontOpenName(display, screen, font_name);
#ifdef HAVE_FONTCONFIG
  // Workaround for Xft crashing the process when trying to render a colored
  // font. See https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=916349 and
  // https://gitlab.freedesktop.org/xorg/lib/libxft/issues/6 among others. In
  // the long run this should be ported to a different font rendering library
  // than Xft.
  FcBool iscol;
  if (xft_font != NULL &&
      FcPatternGetBool(xft_font->pattern, FC_COLOR, 0, &iscol) && iscol) {
    Log("Colored font %s is not supported by Xft", font_name);
    XftFontClose(display, xft_font);
    return NULL;
  }
#else
#warning "Xft enabled without fontconfig. May crash trying to use emoji fonts."
  Log("Xft enabled without fontconfig. May crash trying to use emoji fonts.");
#endif
  return xft_font;
}
#endif

/*! \brief The main program.
 *
 * Usage: XSCREENSAVER_WINDOW=window_id ./auth_x11; status=$?
 *
 * \return 0 if authentication successful, anything else otherwise.
 */
int main(int argc_local, char **argv_local) {
  argc = argc_local;
  argv = argv_local;

  setlocale(LC_CTYPE, "");
  setlocale(LC_TIME, "");

  // This RNG only drives UI jitter, not authentication. An attacker with a
  // screenshot plus an exact startup time and PID could still narrow down the
  // display marker or burn-in offsets, just as they could by recording the
  // screen or keyboard directly.
  struct timeval tv;
  gettimeofday(&tv, NULL);
  SeedPromptRng(&prompt_rng,
                (uint32_t)tv.tv_sec ^ (uint32_t)tv.tv_usec ^
                    (uint32_t)getpid());

  authproto_executable = GetExecutablePathSetting("XSECURELOCK_AUTHPROTO",
                                                  AUTHPROTO_EXECUTABLE, 0);

  // Unless disabled, we shift the login prompt randomly around by a few
  // pixels. This should mostly mitigate burn-in effects from the prompt
  // being displayed all the time, e.g. because the user's mouse is "shivering"
  // and thus the auth prompt reappears soon after timeout.
  burnin_mitigation_max_offset =
      GetIntSetting("XSECURELOCK_BURNIN_MITIGATION", 16);
  if (burnin_mitigation_max_offset > 0) {
    x_offset = RandomRangeInclusive(&prompt_rng, -burnin_mitigation_max_offset,
                                    burnin_mitigation_max_offset);
    y_offset = RandomRangeInclusive(&prompt_rng, -burnin_mitigation_max_offset,
                                    burnin_mitigation_max_offset);
  }

  //! Deprecated flag for setting whether password display should hide the
  //! length.
  int paranoid_password_flag;
  //! Updated flag for password display choice
  const char *password_prompt_flag;

  // If requested, mitigate burn-in even more by moving the auth prompt while
  // displayed. I bet many will find this annoying though.
  burnin_mitigation_max_offset_change =
      GetIntSetting("XSECURELOCK_BURNIN_MITIGATION_DYNAMIC", 0);

  prompt_timeout = GetIntSetting("XSECURELOCK_AUTH_TIMEOUT", 5 * 60);
  show_username = GetIntSetting("XSECURELOCK_SHOW_USERNAME", 1);
  show_hostname = GetIntSetting("XSECURELOCK_SHOW_HOSTNAME", 1);
  paranoid_password_flag = GetIntSetting(
      "XSECURELOCK_" /* REMOVE IN v2 */ "PARANOID_PASSWORD", 1);
  password_prompt_flag = GetStringSetting("XSECURELOCK_PASSWORD_PROMPT", "");
  show_datetime = GetIntSetting("XSECURELOCK_SHOW_DATETIME", 0);
  datetime_format = GetStringSetting("XSECURELOCK_DATETIME_FORMAT", "%c");
  have_switch_user_command =
      !!*GetStringSetting("XSECURELOCK_SWITCH_USER_COMMAND", "");
  auth_sounds = GetIntSetting("XSECURELOCK_AUTH_SOUNDS", 0);
  single_auth_window = GetIntSetting("XSECURELOCK_SINGLE_AUTH_WINDOW", 0);
  auth_cursor_blink = GetIntSetting("XSECURELOCK_AUTH_CURSOR_BLINK", 1);
  auth_padding = GetClampedIntSetting("XSECURELOCK_AUTH_PADDING", 16, 0, INT_MAX);
  auth_border_size =
      GetClampedIntSetting("XSECURELOCK_AUTH_BORDER_SIZE", 0, 0, INT_MAX);
  auth_x_position =
      GetClampedIntSetting("XSECURELOCK_AUTH_X_POSITION", 50, 0, 100);
  auth_y_position =
      GetClampedIntSetting("XSECURELOCK_AUTH_Y_POSITION", 50, 0, 100);
#ifdef HAVE_XKB_EXT
  show_keyboard_layout =
      GetIntSetting("XSECURELOCK_SHOW_KEYBOARD_LAYOUT", 1);
  show_locks_and_latches =
      GetIntSetting("XSECURELOCK_SHOW_LOCKS_AND_LATCHES", 0);
#endif

  password_prompt =
      GetPasswordPromptFromFlags(paranoid_password_flag, password_prompt_flag);

  if ((display = XOpenDisplay(NULL)) == NULL) {
    Log("Could not connect to $DISPLAY");
    return 1;
  }

#ifdef HAVE_XKB_EXT
  int xkb_opcode, xkb_event_base, xkb_error_base;
  int xkb_major_version = XkbMajorVersion, xkb_minor_version = XkbMinorVersion;
  have_xkb_ext =
      XkbQueryExtension(display, &xkb_opcode, &xkb_event_base, &xkb_error_base,
                        &xkb_major_version, &xkb_minor_version);
#endif

  if (!GetHostName(hostname, sizeof(hostname))) {
    return 1;
  }
  if (!GetUserName(username, sizeof(username))) {
    return 1;
  }

  main_window = ReadWindowID();
  if (main_window == None) {
    Log("Invalid/no window ID in XSCREENSAVER_WINDOW");
    return 1;
  }
  Window unused_root;
  Window *unused_children = NULL;
  unsigned int unused_nchildren;
  if (!XQueryTree(display, main_window, &unused_root, &parent_window,
                  &unused_children, &unused_nchildren)) {
    Log("XQueryTree failed for auth window");
    return 1;
  }
  if (unused_children != NULL) {
    XFree(unused_children);
  }
  if (parent_window == None) {
    Log("XQueryTree returned no parent window for auth window");
    return 1;
  }

  Colormap colormap = DefaultColormap(display, DefaultScreen(display));

  const char *background_color =
      GetStringSetting("XSECURELOCK_AUTH_BACKGROUND_COLOR", "black");
  if (!AllocNamedColorOrLog("background", background_color,
                            &xcolor_background)) {
    return 1;
  }
  const char *foreground_color =
      GetStringSetting("XSECURELOCK_AUTH_FOREGROUND_COLOR", "white");
  if (!AllocNamedColorOrLog("foreground", foreground_color,
                            &xcolor_foreground)) {
    return 1;
  }
  const char *warning_color =
      GetStringSetting("XSECURELOCK_AUTH_WARNING_COLOR", "red");
  if (!AllocNamedColorOrLog("warning", warning_color, &xcolor_warning)) {
    return 1;
  }

  core_font = NULL;
#ifdef HAVE_XFT_EXT
  xft_font = NULL;
#endif

  const char *font_name = GetStringSetting("XSECURELOCK_FONT", "");

  // First try parsing the font name as an X11 core font. We're trying these
  // first as their font name format is more restrictive (usually starts with a
  // dash), except for when font aliases are used.
  int have_font = 0;
  if (font_name[0] != 0) {
    core_font = XLoadQueryFont(display, font_name);
    have_font = (core_font != NULL);
#ifdef HAVE_XFT_EXT
    if (!have_font) {
      xft_font =
          FixedXftFontOpenName(display, DefaultScreen(display), font_name);
      have_font = (xft_font != NULL);
    }
#endif
  }
  if (!have_font) {
    if (font_name[0] != 0) {
      Log("Could not load the specified font %s - trying a default font",
          font_name);
    }
#ifdef HAVE_XFT_EXT
    xft_font =
        FixedXftFontOpenName(display, DefaultScreen(display), "monospace");
    have_font = (xft_font != NULL);
#endif
  }
  if (!have_font) {
    core_font = XLoadQueryFont(display, "fixed");
    have_font = (core_font != NULL);
  }
  if (!have_font) {
    Log("Could not load a mind-bogglingly stupid font");
    return 1;
  }

#ifdef HAVE_XFT_EXT
  if (xft_font != NULL) {
    if (!AllocXftColorOrLog("foreground", &xcolor_foreground,
                            &xft_color_foreground,
                            &xft_color_foreground_allocated)) {
      return 1;
    }
    if (!AllocXftColorOrLog("warning", &xcolor_warning, &xft_color_warning,
                            &xft_color_warning_allocated)) {
      XftColorFree(display, DefaultVisual(display, DefaultScreen(display)),
                   DefaultColormap(display, DefaultScreen(display)),
                   &xft_color_foreground);
      xft_color_foreground_allocated = 0;
      return 1;
    }
  }
#endif

  SelectMonitorChangeEvents(display, main_window);

  InitWaitPgrp();

  int status = Authenticate();

  // Clear any possible processing message by closing our windows.
  DestroyPerMonitorWindows(0);

#ifdef HAVE_XFT_EXT
  if (xft_font != NULL) {
    if (xft_color_warning_allocated) {
      XftColorFree(display, DefaultVisual(display, DefaultScreen(display)),
                   DefaultColormap(display, DefaultScreen(display)),
                   &xft_color_warning);
    }
    if (xft_color_foreground_allocated) {
      XftColorFree(display, DefaultVisual(display, DefaultScreen(display)),
                   DefaultColormap(display, DefaultScreen(display)),
                   &xft_color_foreground);
    }
    XftFontClose(display, xft_font);
  }
#endif

  if (core_font != NULL) {
    XFreeFont(display, core_font);
  }

  XFreeColors(display, colormap, &xcolor_warning.pixel, 1, 0);
  XFreeColors(display, colormap, &xcolor_foreground.pixel, 1, 0);
  XFreeColors(display, colormap, &xcolor_background.pixel, 1, 0);

#ifdef HAVE_FONTCONFIG
  FcFini();
#endif
  XCloseDisplay(display);

  return status;
}
