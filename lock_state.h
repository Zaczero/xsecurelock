#ifndef XSECURELOCK_LOCK_STATE_H
#define XSECURELOCK_LOCK_STATE_H

#include <X11/Xlib.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "signal_pipe.h"

#define LOCK_TRACKED_WINDOW_CAPACITY 4

enum WatchChildrenState {
  WATCH_CHILDREN_NORMAL,
  WATCH_CHILDREN_SAVER_DISABLED,
  WATCH_CHILDREN_FORCE_AUTH,
};

struct SensitiveInput {
  XEvent ev;
  char buf[16];
  KeySym keysym;
  int len;
};

struct LockConfig {
  int argc;
  char **argv;
  const char *auth_executable;
  const char *saver_executable;
  char *const *notify_command;
  bool have_switch_user_command;
  int force_grab;
  bool debug_window_info;
  int blank_timeout;
  const char *blank_dpms_state;
  bool saver_reset_on_auth_close;
  int saver_delay_ms;
  bool saver_stop_on_blank;
  const char *background_color;
#ifdef HAVE_XCOMPOSITE_EXT
  bool no_composite;
  bool composite_obscurer;
#endif
};

struct LockBlankingState {
  int64_t time_to_blank_ms;
  bool blanked;
#ifdef HAVE_DPMS_EXT
  bool must_disable_dpms;
#endif
};

struct LockWindows {
  Window tracked_windows[LOCK_TRACKED_WINDOW_CAPACITY];
  size_t tracked_window_count;
  Window root_window;
  Window parent_window;
  Window background_window;
  Window saver_window;
  Window auth_window;
  int width;
  int height;
  Pixmap bg;
  Cursor default_cursor;
  Cursor transparent_cursor;
  Window previous_focused_window;
  int previous_revert_focus_to;
  XIM xim;
  XIC xic;
  bool background_window_mapped;
  bool background_window_visible;
  bool auth_window_mapped;
  bool saver_window_mapped;
  bool xss_lock_notified;
#ifdef HAVE_XCOMPOSITE_EXT
  bool have_xcomposite_ext;
  Window composite_window;
  Window obscurer_window;
  Pixmap obscurer_background_pixmap;
#endif
};

struct LockRuntime {
  Display *display;
  int x11_fd;
  int xss_sleep_lock_fd;
  struct SignalPipe signal_pipe;
  bool need_to_reinstate_grabs;
  pid_t notify_command_pid;
  enum WatchChildrenState xss_requested_saver_state;
  int scrnsaver_event_base;
  int terminate_signal;
  struct SensitiveInput sensitive;
};

struct LockContext {
  struct LockConfig config;
  struct LockBlankingState blanking;
  struct LockWindows windows;
  struct LockRuntime runtime;
};

#endif  // XSECURELOCK_LOCK_STATE_H
