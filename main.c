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

/*!
 *\brief XSecureLock.
 *
 *XSecureLock is an X11 screen lock utility designed with the primary goal of
 *security.
 */

#include "config.h"
#include "build-config.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <errno.h>
#include <fcntl.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_XSCREENSAVER_EXT
#include <X11/extensions/saver.h>
#include <X11/extensions/scrnsaver.h>
#endif
#ifdef HAVE_XF86MISC_EXT
#include <X11/extensions/xf86misc.h>
#endif

#include "auth_child.h"
#include "blanking.h"
#include "configured_command.h"
#include "env_settings.h"
#include "grabs.h"
#include "io_util.h"
#include "lock_state.h"
#include "lock_windows.h"
#include "logging.h"
#include "mlock_page.h"
#include "saver_child.h"
#include "signal_pipe.h"
#include "time_util.h"
#include "util.h"
#include "version.h"
#include "wait_pgrp.h"

#define WATCH_CHILDREN_HZ 10
#define MAX_X_EVENTS_PER_TICK 128

#undef ALWAYS_REINSTATE_GRABS
#undef AUTO_RAISE

static volatile sig_atomic_t g_wakeup_requested = 0;
static volatile sig_atomic_t g_terminate_signal = 0;

static void HandleSIGTERM(int signo) {
  g_terminate_signal = signo;
  SignalPipeNotifyFromHandler();
}

static void HandleSIGUSR2(int unused_signo) {
  (void)unused_signo;
  g_wakeup_requested = 1;
  SignalPipeNotifyFromHandler();
}

static void InitLockContext(struct LockContext *ctx, int argc, char **argv) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->config.argc = argc;
  ctx->config.argv = argv;
  ctx->config.blank_timeout = -1;
  ctx->config.blank_dpms_state = "off";
  ctx->config.background_color = "black";
  ctx->runtime.xss_sleep_lock_fd = -1;
  ctx->runtime.signal_pipe.fds[0] = -1;
  ctx->runtime.signal_pipe.fds[1] = -1;
  ctx->runtime.xss_requested_saver_state = WATCH_CHILDREN_NORMAL;
  ctx->windows.root_window = None;
  ctx->windows.parent_window = None;
  ctx->windows.background_window = None;
  ctx->windows.saver_window = None;
  ctx->windows.auth_window = None;
  ctx->windows.bg = None;
  ctx->windows.default_cursor = None;
  ctx->windows.transparent_cursor = None;
  ctx->windows.previous_focused_window = None;
  ctx->windows.previous_revert_focus_to = RevertToNone;
#ifdef HAVE_XCOMPOSITE_EXT
  ctx->windows.composite_window = None;
  ctx->windows.obscurer_window = None;
#endif
}

static int JustLogErrorsHandler(Display *display, XErrorEvent *error) {
  char buf[128] = "";
  XGetErrorText(display, error->error_code, buf, sizeof(buf));
  buf[sizeof(buf) - 1] = '\0';
  Log("Got non-fatal X11 error: %s", buf);
  return 0;
}

static int SilentlyIgnoreErrorsHandler(Display *display, XErrorEvent *error) {
  (void)display;
  (void)error;
  return 0;
}

static void Version(void) {
  printf("XSecureLock - X11 screen lock utility designed for security.\n");
  if (*git_version) {
    printf("Version: %s\n", git_version);
  } else {
    printf("Version unknown.\n");
  }
}

static void Usage(const char *me) {
  Version();
  printf(
      "\n"
      "Usage:\n"
      "  env [variables...] %s [-- command to run when locked]\n"
      "\n"
      "Environment variables you may set for XSecureLock and its modules:\n"
      "\n"
#include "env_helpstr.inc"  // IWYU pragma: keep
      "\n"
      "Configured default auth module: " AUTH_EXECUTABLE
      "\n"
      "Configured default authproto module: " AUTHPROTO_EXECUTABLE
      "\n"
      "Configured default global saver module: " GLOBAL_SAVER_EXECUTABLE
      "\n"
      "Configured default per-screen saver module: " SAVER_EXECUTABLE
      "\n"
      "\n"
      "This software is licensed under the Apache 2.0 License. Details are\n"
      "available at the following location:\n"
      "  " DOCS_PATH "/COPYING\n",
      me,
      "%s",
      "%s");
}

static void LoadDefaults(struct LockConfig *config) {
  config->auth_executable =
      GetExecutablePathSetting("XSECURELOCK_AUTH", AUTH_EXECUTABLE, 1);
  config->saver_executable = GetExecutablePathSetting(
      "XSECURELOCK_GLOBAL_SAVER", GLOBAL_SAVER_EXECUTABLE, 0);
  config->have_switch_user_command =
      GetStringSetting("XSECURELOCK_SWITCH_USER_COMMAND", "")[0] != '\0';
  config->force_grab = GetIntSetting("XSECURELOCK_FORCE_GRAB", 0);
  config->debug_window_info =
      GetBoolSetting("XSECURELOCK_DEBUG_WINDOW_INFO", 0);
  config->blank_timeout = GetIntSetting("XSECURELOCK_BLANK_TIMEOUT", 600);
  config->blank_dpms_state =
      GetStringSetting("XSECURELOCK_BLANK_DPMS_STATE", "off");
  config->saver_reset_on_auth_close =
      GetBoolSetting("XSECURELOCK_SAVER_RESET_ON_AUTH_CLOSE", 0);
  config->saver_delay_ms =
      GetNonnegativeIntSetting("XSECURELOCK_SAVER_DELAY_MS", 0);
  config->saver_stop_on_blank =
      GetBoolSetting("XSECURELOCK_SAVER_STOP_ON_BLANK", 1);
  config->background_color =
      GetStringSetting("XSECURELOCK_BACKGROUND_COLOR", "black");
#ifdef HAVE_XCOMPOSITE_EXT
  config->no_composite = GetBoolSetting("XSECURELOCK_NO_COMPOSITE", 0);
  config->composite_obscurer =
      GetBoolSetting("XSECURELOCK_COMPOSITE_OBSCURER", 1);
#endif
}

static void ParseArgumentsOrExit(struct LockConfig *config) {
  for (int i = 1; i < config->argc; ++i) {
    if (!strncmp(config->argv[i], "auth_", 5)) {
      Log("Setting auth child name from command line is DEPRECATED. Use "
          "the XSECURELOCK_AUTH environment variable instead");
      config->auth_executable = config->argv[i];
      continue;
    }
    if (!strncmp(config->argv[i], "saver_", 6)) {
      Log("Setting saver child name from command line is DEPRECATED. Use "
          "the XSECURELOCK_SAVER environment variable instead");
      config->saver_executable = config->argv[i];
      continue;
    }
    if (!strcmp(config->argv[i], "--")) {
      config->notify_command = config->argv + i + 1;
      break;
    }
    if (!strcmp(config->argv[i], "--help")) {
      Usage(config->argv[0]);
      exit(EXIT_SUCCESS);
    }
    if (!strcmp(config->argv[i], "--version")) {
      Version();
      exit(EXIT_SUCCESS);
    }
    Log("Unrecognized argument: %s", config->argv[i]);
    Usage(config->argv[0]);
    exit(EXIT_SUCCESS);
  }
}

static int CheckSettings(const struct LockConfig *config) {
  if (config->auth_executable == NULL) {
    Log("Auth module has not been specified in any way");
    return 0;
  }
  if (config->saver_executable == NULL) {
    Log("Saver module has not been specified in any way");
    return 0;
  }
  return 1;
}

static int CheckLockingEffectiveness(void) {
  int error_status = 0;
  const char *error_string = "Will not lock";

  if (GetBoolSetting("XSECURELOCK_DEBUG_ALLOW_LOCKING_IF_INEFFECTIVE", 0)) {
    error_status = 1;
    error_string = "Locking anyway";
  }
  if (*GetStringSetting("WAYLAND_DISPLAY", "")) {
    Log("Wayland detected. This would only lock the X11 part of your session. "
        "%s",
        error_string);
    return error_status;
  }
  if (*GetStringSetting("VNCDESKTOP", "")) {
    Log("VNC detected. This would only lock your remote session. %s",
        error_string);
    return error_status;
  }
  if (*GetStringSetting("CHROME_REMOTE_DESKTOP_SESSION", "")) {
    Log("Chrome Remote Desktop detected. This would only lock your remote "
        "session. %s",
        error_string);
    return error_status;
  }
  return 1;
}

static int WatchChildren(struct LockContext *ctx, enum WatchChildrenState state,
                         const char *stdinbuf) {
  int want_auth = WantAuthChild(state == WATCH_CHILDREN_FORCE_AUTH);
  int auth_running = 0;

  if (want_auth) {
    if (WatchAuthChild(ctx->windows.auth_window, ctx->config.auth_executable,
                       state == WATCH_CHILDREN_FORCE_AUTH, stdinbuf,
                       &auth_running)) {
      WatchSaverChild(ctx->runtime.display, ctx->windows.saver_window, 0,
                      ctx->config.saver_executable, 0);
      return 1;
    }
    if (!auth_running) {
      XUnmapWindow(ctx->runtime.display, ctx->windows.auth_window);
      if (ctx->config.saver_reset_on_auth_close) {
        KillAllSaverChildrenSigHandler(SIGUSR1);
      }
    }
  }

  WatchSaverChild(ctx->runtime.display, ctx->windows.saver_window, 0,
                  ctx->config.saver_executable,
                  state != WATCH_CHILDREN_SAVER_DISABLED);

  if (auth_running) {
    LockUnblankScreen(ctx);
  } else {
    LockMaybeBlankScreen(ctx);
  }
  return 0;
}

static int WakeUp(struct LockContext *ctx, const char *stdinbuf) {
  return WatchChildren(ctx, WATCH_CHILDREN_FORCE_AUTH, stdinbuf);
}

static void NotifyOfLock(struct LockContext *ctx) {
  (void)CloseIfValid(&ctx->runtime.xss_sleep_lock_fd);

  if (ctx->config.notify_command == NULL || *ctx->config.notify_command == NULL) {
    return;
  }

  pid_t pid = ForkWithoutSigHandlers();
  if (pid == -1) {
    LogErrno("fork");
  } else if (pid == 0) {
    execvp(ctx->config.notify_command[0], ctx->config.notify_command);
    LogErrno("execvp");
    _exit(EXIT_FAILURE);
  } else {
    ctx->runtime.notify_command_pid = pid;
  }
}

static void ReapNotifyCommand(struct LockContext *ctx) {
  if (ctx->runtime.notify_command_pid == 0) {
    return;
  }

  int status = 0;
  WaitProc("notify", &ctx->runtime.notify_command_pid, 0, 0, &status);
}

static void MakeSleepLockFdCloexec(int fd) {
  if (fd == -1) {
    return;
  }
  if (SetFdCloexec(fd) == -1) {
    LogErrno("fcntl(XSS_SLEEP_LOCK_FD)");
  }
}

static int InstallSignalHandlers(void) {
  struct sigaction sa = {.sa_flags = 0, .sa_handler = SIG_IGN};
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGPIPE, &sa, NULL) != 0) {
    LogErrno("sigaction(SIGPIPE)");
    return 0;
  }

  sa.sa_handler = HandleSIGUSR2;
  if (sigaction(SIGUSR2, &sa, NULL) != 0) {
    LogErrno("sigaction(SIGUSR2)");
    return 0;
  }

  sa.sa_flags = SA_RESETHAND;
  sa.sa_handler = HandleSIGTERM;
  if (sigaction(SIGTERM, &sa, NULL) != 0) {
    LogErrno("sigaction(SIGTERM)");
    return 0;
  }
  return 1;
}

static int HandleSignalFlags(struct LockContext *ctx) {
  if (g_terminate_signal != 0) {
    ctx->runtime.terminate_signal = g_terminate_signal;
    g_terminate_signal = 0;
    return 1;
  }
  if (!g_wakeup_requested) {
    return 0;
  }

  g_wakeup_requested = 0;
#ifdef DEBUG_EVENTS
  Log("WakeUp on signal");
#endif
  LockUnblankScreen(ctx);
  return WakeUp(ctx, NULL);
}

static int HandlePointerWakeEvent(struct LockContext *ctx) {
  LockScreenNoLongerBlanked(ctx);
  return WakeUp(ctx, NULL);
}

static bool LookupKeypress(struct LockContext *ctx) {
  Status lookup_status = XLookupNone;
  ctx->runtime.sensitive.keysym = NoSymbol;

  if (ctx->windows.xic != NULL) {
    ctx->runtime.sensitive.len = XmbLookupString(
        ctx->windows.xic, &ctx->runtime.sensitive.ev.xkey,
        ctx->runtime.sensitive.buf, sizeof(ctx->runtime.sensitive.buf) - 1,
        &ctx->runtime.sensitive.keysym, &lookup_status);
    if (ctx->runtime.sensitive.len <= 0) {
      return false;
    } else if (lookup_status != XLookupChars &&
               lookup_status != XLookupBoth) {
      return false;
    }
  } else {
    ctx->runtime.sensitive.len = XLookupString(
        &ctx->runtime.sensitive.ev.xkey, ctx->runtime.sensitive.buf,
        sizeof(ctx->runtime.sensitive.buf) - 1, &ctx->runtime.sensitive.keysym,
        NULL);
    if (ctx->runtime.sensitive.len <= 0) {
      return false;
    }
  }

  if ((size_t)ctx->runtime.sensitive.len >= sizeof(ctx->runtime.sensitive.buf)) {
    Log("Received invalid length from XLookupString: %d",
        ctx->runtime.sensitive.len);
    return false;
  }
  return true;
}

static bool ApplyControlKeyTranslations(struct LockContext *ctx) {
  if (ctx->runtime.sensitive.keysym == XK_Tab &&
      (ctx->runtime.sensitive.ev.xkey.state & ControlMask)) {
    ctx->runtime.sensitive.buf[0] = '\023';
    ctx->runtime.sensitive.buf[1] = '\0';
    return true;
  }
  if (ctx->runtime.sensitive.keysym == XK_BackSpace &&
      (ctx->runtime.sensitive.ev.xkey.state & ControlMask)) {
    ctx->runtime.sensitive.buf[0] = '\025';
    ctx->runtime.sensitive.buf[1] = '\0';
    return true;
  }
  if (ctx->config.have_switch_user_command &&
      (ctx->runtime.sensitive.keysym == XK_o ||
       ctx->runtime.sensitive.keysym == XK_0) &&
      (((ctx->runtime.sensitive.ev.xkey.state & ControlMask) &&
        (ctx->runtime.sensitive.ev.xkey.state & Mod1Mask)) ||
       (ctx->runtime.sensitive.ev.xkey.state & Mod4Mask))) {
    (void)RunShellCommandFromEnv("XSECURELOCK_SWITCH_USER_COMMAND", 1);
    ctx->runtime.sensitive.buf[0] = '\025';
    ctx->runtime.sensitive.buf[1] = '\0';
    return true;
  }
  return false;
}

static bool RunConfiguredKeyCommandIfAny(struct LockContext *ctx) {
  const char *keyname = XKeysymToString(ctx->runtime.sensitive.keysym);
  if (keyname == NULL) {
    return false;
  }

  char env_name[64];
  if (!FormatKeyCommandEnvName(env_name, sizeof(env_name), keyname)) {
    return false;
  }

  const char *command = GetStringSetting(env_name, "");
  if (*command == '\0') {
    return false;
  }

  (void)RunShellCommandFromEnv(env_name, 1);
  return true;
}

static int HandleKeyPressEvent(struct LockContext *ctx) {
  bool have_key = false;
  bool do_wake_up = true;

  LockScreenNoLongerBlanked(ctx);
  have_key = LookupKeypress(ctx);

  if (!ApplyControlKeyTranslations(ctx)) {
    if (have_key) {
      if (ctx->runtime.sensitive.len == 1 &&
          ctx->runtime.sensitive.buf[0] == '\r') {
        ctx->runtime.sensitive.buf[0] = '\n';
      }
      ctx->runtime.sensitive.buf[ctx->runtime.sensitive.len] = '\0';
    } else {
      ctx->runtime.sensitive.buf[0] = '\0';
      do_wake_up = !RunConfiguredKeyCommandIfAny(ctx);
    }
  }

  int authenticated =
      do_wake_up ? WakeUp(ctx, ctx->runtime.sensitive.buf) : 0;
  explicit_bzero(&ctx->runtime.sensitive, sizeof(ctx->runtime.sensitive));
  return authenticated;
}

static int HandleFocusEvent(struct LockContext *ctx) {
  if (ctx->runtime.sensitive.ev.xfocus.window != ctx->windows.root_window ||
      ctx->runtime.sensitive.ev.xfocus.mode != NotifyUngrab) {
    return 0;
  }

  if (!LockAcquireGrabs(ctx, 0, 0)) {
    Log("Critical: could not reacquire grabs after NotifyUngrab. The screen is "
        "now UNLOCKED! Trying again next frame.");
    ctx->runtime.need_to_reinstate_grabs = true;
  }
  return 0;
}

static void HandleClientMessage(struct LockContext *ctx) {
  if (ctx->runtime.sensitive.ev.xclient.window == ctx->windows.root_window) {
    return;
  }

#ifdef DEBUG_EVENTS
  char *message_type = XGetAtomName(
      ctx->runtime.display, ctx->runtime.sensitive.ev.xclient.message_type);
  Log("Received unexpected ClientMessage event %s on window %lu",
      message_type == NULL ? "(null)" : message_type,
      ctx->runtime.sensitive.ev.xclient.window);
  if (message_type != NULL) {
    XFree(message_type);
  }
#endif
}

static int HandleScreenSaverEvent(struct LockContext *ctx) {
#ifdef HAVE_XSCREENSAVER_EXT
  if (ctx->runtime.scrnsaver_event_base != 0 &&
      ctx->runtime.sensitive.ev.type ==
          ctx->runtime.scrnsaver_event_base + ScreenSaverNotify) {
    XScreenSaverNotifyEvent *xss_ev =
        (XScreenSaverNotifyEvent *)&ctx->runtime.sensitive.ev;
    ctx->runtime.xss_requested_saver_state =
        xss_ev->state == ScreenSaverOn ? WATCH_CHILDREN_SAVER_DISABLED
                                       : WATCH_CHILDREN_NORMAL;
    return 1;
  }
#endif
  return 0;
}

static int ProcessOneXEvent(struct LockContext *ctx) {
  switch (ctx->runtime.sensitive.ev.type) {
    case ConfigureNotify:
      LockWindowsHandleConfigureNotify(ctx,
                                       &ctx->runtime.sensitive.ev.xconfigure);
      return 0;
    case VisibilityNotify:
      LockWindowsHandleVisibilityNotify(ctx,
                                        &ctx->runtime.sensitive.ev.xvisibility);
      return 0;
    case MotionNotify:
    case ButtonPress:
      return HandlePointerWakeEvent(ctx);
    case KeyPress:
      return HandleKeyPressEvent(ctx);
    case KeyRelease:
    case ButtonRelease:
      LockScreenNoLongerBlanked(ctx);
      return 0;
    case MappingNotify:
    case EnterNotify:
    case LeaveNotify:
      return 0;
    case MapNotify:
      LockWindowsHandleMapNotify(ctx, &ctx->runtime.sensitive.ev.xmap);
      return 0;
    case UnmapNotify:
      LockWindowsHandleUnmapNotify(ctx, &ctx->runtime.sensitive.ev.xunmap);
      return 0;
    case FocusIn:
    case FocusOut:
      return HandleFocusEvent(ctx);
    case ClientMessage:
      HandleClientMessage(ctx);
      return 0;
    default:
      if (HandleScreenSaverEvent(ctx)) {
        return 0;
      }
      Log("Received unexpected event %d", ctx->runtime.sensitive.ev.type);
      return 0;
  }
}

static int ProcessPendingXEvents(struct LockContext *ctx, int *more_pending) {
  int drained = 0;

  while (drained < MAX_X_EVENTS_PER_TICK &&
         XPending(ctx->runtime.display) > 0) {
    XNextEvent(ctx->runtime.display, &ctx->runtime.sensitive.ev);
    ++drained;

    if (XFilterEvent(&ctx->runtime.sensitive.ev, None)) {
      continue;
    }
    if (ProcessOneXEvent(ctx)) {
      return 1;
    }
    if (LockWindowsReadyToNotify(ctx) && !ctx->windows.xss_lock_notified) {
      NotifyOfLock(ctx);
      LockWindowsMarkNotified(ctx);
    }
  }

  *more_pending = XPending(ctx->runtime.display) > 0;
  return 0;
}

static int AcquireInitialGrabs(struct LockContext *ctx) {
  int last_normal_attempt = ctx->config.force_grab ? 1 : 0;
  int retries = 10;

  for (; retries >= 0; --retries) {
    if (LockAcquireGrabs(ctx, retries > last_normal_attempt,
                         retries < last_normal_attempt)) {
      return 1;
    }
    if (ctx->windows.previous_focused_window == None) {
      XGetInputFocus(ctx->runtime.display, &ctx->windows.previous_focused_window,
                     &ctx->windows.previous_revert_focus_to);
      XSetInputFocus(ctx->runtime.display, PointerRoot, RevertToPointerRoot,
                     CurrentTime);
      XFlush(ctx->runtime.display);
    }
    (void)SleepMs(100);
  }

  Log("Failed to grab. Giving up.");
  return 0;
}

static int InitializeLockContextRuntime(struct LockContext *ctx) {
  if (MLOCK_PAGE(&ctx->runtime.sensitive, sizeof(ctx->runtime.sensitive)) < 0) {
    LogErrno("mlock");
  }

  if (SignalPipeInit(&ctx->runtime.signal_pipe) != 0) {
    LogErrno("SignalPipeInit");
    return 0;
  }
  SignalPipeSetWriteFdForHandler(ctx->runtime.signal_pipe.fds[1]);

  if (!InstallSignalHandlers()) {
    return 0;
  }

  InitWaitPgrp();
  XFlush(ctx->runtime.display);

#ifdef HAVE_XSCREENSAVER_EXT
  if (ctx->runtime.scrnsaver_event_base != 0) {
    XScreenSaverInfo *info = XScreenSaverAllocInfo();
    if (info == NULL) {
      Log("XScreenSaverAllocInfo failed");
    } else {
      if (!XScreenSaverQueryInfo(ctx->runtime.display, ctx->windows.root_window,
                                 info)) {
        Log("XScreenSaverQueryInfo failed");
      } else if (info->state == ScreenSaverOn &&
                 info->kind == ScreenSaverBlanked &&
                 ctx->config.saver_stop_on_blank) {
        ctx->runtime.xss_requested_saver_state =
            WATCH_CHILDREN_SAVER_DISABLED;
      }
      XFree(info);
    }
  }
#endif

  LockBlankingInit(ctx);
  return 1;
}

static int RunLockMainLoop(struct LockContext *ctx, int *have_pending_x_events) {
  XFlush(ctx->runtime.display);
  if (WatchChildren(ctx, ctx->runtime.xss_requested_saver_state, NULL)) {
    return 1;
  }

  (void)SleepMs(ctx->config.saver_delay_ms);
  LockWindowsMap(ctx);
  XSetErrorHandler(JustLogErrorsHandler);

  ctx->runtime.x11_fd = ConnectionNumber(ctx->runtime.display);
  if (ctx->runtime.x11_fd == ctx->runtime.xss_sleep_lock_fd &&
      ctx->runtime.xss_sleep_lock_fd != -1) {
    Log("XSS_SLEEP_LOCK_FD matches DISPLAY - what?!? We're probably "
        "inhibiting sleep now");
    ctx->runtime.xss_sleep_lock_fd = -1;
  }

  for (;;) {
    struct pollfd fds[2] = {
        {.fd = ctx->runtime.x11_fd, .events = POLLIN | POLLHUP, .revents = 0},
        {.fd = ctx->runtime.signal_pipe.fds[0],
         .events = POLLIN | POLLHUP,
         .revents = 0},
    };
    int ready =
        RetryPoll(fds, ARRAY_LEN(fds),
                  *have_pending_x_events ? 0 : 1000 / WATCH_CHILDREN_HZ);
    if (ready < 0) {
      LogErrno("poll");
    }

    if (fds[1].revents & (POLLIN | POLLHUP)) {
      SignalPipeDrain(ctx->runtime.signal_pipe.fds[0], "signal pipe");
    }
    if (HandleSignalFlags(ctx)) {
      return 1;
    }

    enum WatchChildrenState requested_saver_state =
        (ctx->config.saver_stop_on_blank && ctx->blanking.blanked)
            ? WATCH_CHILDREN_SAVER_DISABLED
            : ctx->runtime.xss_requested_saver_state;
    if (WatchChildren(ctx, requested_saver_state, NULL)) {
      return 1;
    }

    XUndefineCursor(ctx->runtime.display, ctx->windows.saver_window);

#ifdef ALWAYS_REINSTATE_GRABS
    ctx->runtime.need_to_reinstate_grabs = true;
#endif
    if (ctx->runtime.need_to_reinstate_grabs) {
      ctx->runtime.need_to_reinstate_grabs = false;
      if (!LockAcquireGrabs(ctx, 0, 0)) {
        Log("Critical: could not reacquire grabs. The screen is now UNLOCKED! "
            "Trying again next frame.");
        ctx->runtime.need_to_reinstate_grabs = true;
      }
    }

#ifdef AUTO_RAISE
    if (ctx->windows.auth_window_mapped) {
      LockMaybeRaiseWindow(ctx, ctx->windows.auth_window, 0, 0);
    }
    LockMaybeRaiseWindow(ctx, ctx->windows.background_window, 0, 0);
#ifdef HAVE_XCOMPOSITE_EXT
    if (ctx->windows.obscurer_window != None) {
      LockMaybeRaiseWindow(ctx, ctx->windows.obscurer_window, 1, 0);
    }
#endif
#endif

    ReapNotifyCommand(ctx);

    if (ProcessPendingXEvents(ctx, have_pending_x_events)) {
      return 1;
    }
  }
}

static void CleanupLockContextRuntime(struct LockContext *ctx) {
  if (ctx->runtime.terminate_signal != 0) {
    KillAllSaverChildrenSigHandler(ctx->runtime.terminate_signal);
    KillAuthChildSigHandler(ctx->runtime.terminate_signal);
  }

  if (ctx->runtime.display != NULL) {
    LockUnblankScreen(ctx);
  }

  if (ctx->runtime.display != NULL &&
      ctx->windows.previous_focused_window != None) {
    XSetErrorHandler(SilentlyIgnoreErrorsHandler);
    XSetInputFocus(ctx->runtime.display, ctx->windows.previous_focused_window,
                   ctx->windows.previous_revert_focus_to, CurrentTime);
    XSetErrorHandler(JustLogErrorsHandler);
  }

  explicit_bzero(&ctx->runtime.sensitive, sizeof(ctx->runtime.sensitive));
  (void)CloseIfValid(&ctx->runtime.xss_sleep_lock_fd);
  SignalPipeClose(&ctx->runtime.signal_pipe);
  LockWindowsCleanup(ctx);
  if (ctx->runtime.display != NULL) {
    XCloseDisplay(ctx->runtime.display);
    ctx->runtime.display = NULL;
  }
}

int main(int argc, char **argv) {
  struct LockContext ctx;
  int status = EXIT_FAILURE;
  int have_pending_x_events = 0;

  setlocale(LC_CTYPE, "");
  InitLockContext(&ctx, argc, argv);

  ctx.runtime.xss_sleep_lock_fd = GetIntSetting("XSS_SLEEP_LOCK_FD", -1);
  MakeSleepLockFdCloexec(ctx.runtime.xss_sleep_lock_fd);

  if (chdir("/")) {
    Log("Could not switch to the root directory");
    goto done;
  }
  if (access(HELPER_PATH "/", X_OK)) {
    Log("Could not access directory %s", HELPER_PATH);
    goto done;
  }

  LoadDefaults(&ctx.config);
  ParseArgumentsOrExit(&ctx.config);
  if (!CheckSettings(&ctx.config)) {
    Usage(ctx.config.argv[0]);
    goto done;
  }
  if (!CheckLockingEffectiveness()) {
    goto done;
  }

  ctx.runtime.display = XOpenDisplay(NULL);
  if (ctx.runtime.display == NULL) {
    Log("Could not connect to $DISPLAY");
    goto done;
  }

  if (ScreenCount(ctx.runtime.display) != 1) {
    Log("Warning: 'Zaphod' configurations are not supported at this point. "
        "Only locking the default screen.\n");
  }

  if (!LockWindowsInit(&ctx)) {
    goto done;
  }

#ifdef HAVE_XSCREENSAVER_EXT
  {
    int scrnsaver_error_base = 0;
    if (!XScreenSaverQueryExtension(ctx.runtime.display,
                                    &ctx.runtime.scrnsaver_event_base,
                                    &scrnsaver_error_base)) {
      ctx.runtime.scrnsaver_event_base = 0;
    }
    XScreenSaverSelectInput(ctx.runtime.display, ctx.windows.background_window,
                            ScreenSaverNotifyMask);
  }
#endif

#ifdef HAVE_XF86MISC_EXT
  if (XF86MiscSetGrabKeysState(ctx.runtime.display, False) !=
      MiscExtGrabStateSuccess) {
    Log("Could not set grab keys state");
    goto done;
  }
#endif

  if (!AcquireInitialGrabs(&ctx)) {
    goto done;
  }

  if (!InitializeLockContextRuntime(&ctx)) {
    goto done;
  }

  status = EXIT_SUCCESS;
  if (RunLockMainLoop(&ctx, &have_pending_x_events)) {
    goto done;
  }

done:
  CleanupLockContextRuntime(&ctx);

  if (ctx.runtime.terminate_signal != 0) {
    struct sigaction sa = {.sa_flags = 0, .sa_handler = SIG_DFL};
    sigemptyset(&sa.sa_mask);
    (void)sigaction(ctx.runtime.terminate_signal, &sa, NULL);
    raise(ctx.runtime.terminate_signal);
  }

  return status;
}
