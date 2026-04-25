/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*!
 * \brief Screen dimmer helper.
 *
 * A simple tool to run a tool to dim the screen, and - depending on which comes
 * first:
 * - On leaving idle status, kill the dimming tool and exit with success status.
 * - On dimming tool exiting, exit with error status.
 *
 * Sample usage:
 *   until_nonidle dim-screen || xsecurelock
 */

#include "config.h"

#include <errno.h>     // for errno, ESRCH
#include <X11/X.h>     // for Window
#include <X11/Xlib.h>  // for Display, XOpenDisplay, Default...
#include <signal.h>    // for sigaction, raise, sigemptyset
#include <stdbool.h>   // for bool
#include <stdint.h>    // for uint64_t
#include <stdlib.h>    // for EXIT_FAILURE
#include <string.h>    // for memcpy, strcmp, strcspn
#include <unistd.h>    // for _exit, execvp

#ifdef HAVE_XSCREENSAVER_EXT
#include <X11/extensions/scrnsaver.h>  // for XScreenSaverAllocInfo, XScreen...
#endif

#ifdef HAVE_XSYNC_EXT
#include <X11/extensions/sync.h>       // for XSyncSystemCounter, XSyncListS...
#include <X11/extensions/syncconst.h>  // for XSyncValue
#endif

#include "../env_settings.h"  // for GetNonnegativeIntSetting, GetStringSetting
#include "../logging.h"       // for Log, LogErrno
#include "../time_util.h"     // for GetMonotonicTimeMs, SleepMs
#include "../wait_pgrp.h"     // for KillPgrp, WaitPgrp

#define MAX_IDLE_TIMERS 16

struct UntilNonidleState {
  Display *display;
  Window root_window;
  pid_t childpid;
  int dim_time_ms;
  int wait_time_ms;
  const char *timers;
  uint64_t prev_idle;
  int64_t start_time_ms;
  bool still_idle;
  int terminate_signal;
  int status;
#ifdef HAVE_XSCREENSAVER_EXT
  bool have_xscreensaver_ext;
  XScreenSaverInfo *saver_info;
#endif
#ifdef HAVE_XSYNC_EXT
  bool have_xsync_ext;
  int num_xsync_counters;
  XSyncSystemCounter *xsync_counters;
#endif
};

static volatile sig_atomic_t g_terminate_signal = 0;

static void HandleSIGTERM(int signo) { g_terminate_signal = signo; }

static void InitState(struct UntilNonidleState *state) {
  memset(state, 0, sizeof(*state));
  state->status = EXIT_FAILURE;
  state->still_idle = true;
}

static void InitializeExtensions(struct UntilNonidleState *state) {
#ifdef HAVE_XSCREENSAVER_EXT
  int scrnsaver_event_base = 0;
  int scrnsaver_error_base = 0;
  if (XScreenSaverQueryExtension(state->display, &scrnsaver_event_base,
                                 &scrnsaver_error_base)) {
    state->saver_info = XScreenSaverAllocInfo();
    if (state->saver_info != NULL) {
      state->have_xscreensaver_ext = true;
    } else {
      Log("XScreenSaverAllocInfo failed; disabling XScreenSaver idle timer");
    }
  }
#endif

#ifdef HAVE_XSYNC_EXT
  int sync_event_base = 0;
  int sync_error_base = 0;
  if (XSyncQueryExtension(state->display, &sync_event_base, &sync_error_base)) {
    state->xsync_counters =
        XSyncListSystemCounters(state->display, &state->num_xsync_counters);
    if (state->xsync_counters != NULL) {
      state->have_xsync_ext = true;
    } else {
      Log("XSyncListSystemCounters failed; disabling XSync idle timers");
    }
  }
#endif
}

static uint64_t GetIdleTimeForSingleTimer(const struct UntilNonidleState *state,
                                          const char *timer) {
  if (*timer == '\0') {
#ifdef HAVE_XSCREENSAVER_EXT
    if (state->have_xscreensaver_ext) {
      if (!XScreenSaverQueryInfo(state->display, state->root_window,
                                 state->saver_info)) {
        Log("XScreenSaverQueryInfo failed");
        return (uint64_t)-1;
      }
      return state->saver_info->idle;
    }
#endif
  } else {
#ifdef HAVE_XSYNC_EXT
    if (state->have_xsync_ext) {
      for (int i = 0; i < state->num_xsync_counters; ++i) {
        if (!strcmp(timer, state->xsync_counters[i].name)) {
          XSyncValue value;
          if (!XSyncQueryCounter(state->display,
                                 state->xsync_counters[i].counter, &value)) {
            Log("XSyncQueryCounter failed for timer \"%s\"", timer);
            return (uint64_t)-1;
          }
          return (((uint64_t)XSyncValueHigh32(value)) << 32) |
                 (uint64_t)XSyncValueLow32(value);
        }
      }
    }
#endif
  }

  Log("Timer \"%s\" not supported", timer);
  return (uint64_t)-1;
}

static uint64_t GetIdleTime(const struct UntilNonidleState *state) {
  const char *timers = state->timers;
  uint64_t min_idle_time = (uint64_t)-1;
  size_t timer_count = 0;

  for (;;) {
    char timer_name[64];
    size_t len = strcspn(timers, ",");

    if (timer_count == MAX_IDLE_TIMERS) {
      Log("Too many idle timers configured; ignoring extras after %d entries",
          MAX_IDLE_TIMERS);
      return min_idle_time;
    }
    ++timer_count;

    if (len < sizeof(timer_name)) {
      memcpy(timer_name, timers, len);
      timer_name[len] = '\0';
      {
        uint64_t this_idle_time = GetIdleTimeForSingleTimer(state, timer_name);
        if (this_idle_time < min_idle_time) {
          min_idle_time = this_idle_time;
        }
      }
    } else {
      Log("Too long timer name - skipping: %s", timers);
    }

    if (timers[len] == '\0') {
      return min_idle_time;
    }
    timers += len + 1;
  }
}

static int SpawnChild(struct UntilNonidleState *state, char **argv) {
  state->childpid = ForkWithoutSigHandlers();
  if (state->childpid == -1) {
    LogErrno("fork");
    return 0;
  }
  if (state->childpid != 0) {
    return 1;
  }

  StartPgrp();
  execvp(argv[1], argv + 1);
  LogErrno("execvp");
  _exit(EXIT_FAILURE);
}

static int InstallSignalHandlers(void) {
  struct sigaction sa = {.sa_flags = SA_RESETHAND, .sa_handler = HandleSIGTERM};
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGTERM, &sa, NULL) != 0) {
    LogErrno("sigaction(SIGTERM)");
    return 0;
  }
  return 1;
}

static int UpdateIdleStatus(struct UntilNonidleState *state) {
  uint64_t cur_idle = GetIdleTime(state);
  if (cur_idle == (uint64_t)-1) {
    return 0;
  }

  state->still_idle = cur_idle >= state->prev_idle;
  state->prev_idle = cur_idle;
  return 1;
}

int main(int argc, char **argv) {
  struct UntilNonidleState state;

  if (argc <= 1) {
    Log("Usage: %s program args... - runs the given program until non-idle",
        argv[0]);
    Log("Meant to be used with dimming tools, like: %s dimmer || xsecurelock",
        argv[0]);
    Log("Returns 0 when no longer idle, and 1 when still idle");
    return 1;
  }

  InitState(&state);
  state.dim_time_ms = GetNonnegativeIntSetting("XSECURELOCK_DIM_TIME_MS", 2000);
  state.wait_time_ms =
      GetNonnegativeIntSetting("XSECURELOCK_WAIT_TIME_MS", 5000);
  state.timers = GetStringSetting("XSECURELOCK_IDLE_TIMERS",
#ifdef HAVE_XSCREENSAVER_EXT
                                  ""
#else
                                  "IDLETIME"
#endif
  );

  state.display = XOpenDisplay(NULL);
  if (state.display == NULL) {
    Log("Could not connect to $DISPLAY.");
    goto done;
  }
  state.root_window = DefaultRootWindow(state.display);

  InitializeExtensions(&state);
  state.prev_idle = GetIdleTime(&state);
  if (state.prev_idle == (uint64_t)-1) {
    Log("Could not initialize idle timers. Bailing out.");
    goto done;
  }

  if (!SpawnChild(&state, argv)) {
    goto done;
  }
  if (!InstallSignalHandlers()) {
    goto done;
  }

  InitWaitPgrp();
  if (GetMonotonicTimeMs(&state.start_time_ms) != 0) {
    LogErrno("GetMonotonicTimeMs");
    goto done;
  }

  while (state.childpid != 0) {
    if (g_terminate_signal != 0) {
      state.terminate_signal = g_terminate_signal;
      g_terminate_signal = 0;
      goto done;
    }

    (void)SleepMs(10);

    if (g_terminate_signal != 0) {
      state.terminate_signal = g_terminate_signal;
      g_terminate_signal = 0;
      goto done;
    }
    if (!UpdateIdleStatus(&state)) {
      Log("Could not read idle timers");
      goto done;
    }

    {
      int64_t current_time_ms = 0;
      if (GetMonotonicTimeMs(&current_time_ms) != 0) {
        LogErrno("GetMonotonicTimeMs");
        goto done;
      }

      const bool should_be_running =
          state.still_idle &&
          (current_time_ms - state.start_time_ms <=
           (int64_t)state.dim_time_ms + (int64_t)state.wait_time_ms);
      if (!should_be_running && state.childpid != 0 &&
          KillPgrp(state.childpid, SIGTERM) != 0 && errno != ESRCH) {
        LogErrno("KillPgrp");
      }

      int status = 0;
      (void)WaitPgrp("idle", &state.childpid, 0, !should_be_running, &status);
    }
  }

  state.status = state.still_idle ? 1 : 0;

done:
  if (state.childpid != 0) {
    int status = 0;
    (void)KillPgrp(state.childpid, SIGTERM);
    (void)WaitPgrp("idle", &state.childpid, 1, 1, &status);
  }

#ifdef HAVE_XSYNC_EXT
  if (state.xsync_counters != NULL) {
    XSyncFreeSystemCounterList(state.xsync_counters);
  }
#endif
#ifdef HAVE_XSCREENSAVER_EXT
  if (state.saver_info != NULL) {
    XFree(state.saver_info);
  }
#endif
  if (state.display != NULL) {
    XCloseDisplay(state.display);
  }

  if (state.terminate_signal != 0) {
    struct sigaction sa = {.sa_flags = 0, .sa_handler = SIG_DFL};
    sigemptyset(&sa.sa_mask);
    (void)sigaction(state.terminate_signal, &sa, NULL);
    (void)raise(state.terminate_signal);
  }

  return state.status;
}
