/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"
#include "build-config.h"

#include <X11/X.h>     // for Window, CopyFromParent
#include <X11/Xlib.h>  // for XEvent, XFlush, XNextEvent, XOpenDi...
#include <errno.h>     // for errno, EAGAIN, EINTR, EWOULDBLOCK
#include <poll.h>      // for pollfd, POLLIN, POLLHUP
#include <signal.h>    // for sigaction, SIGCHLD, SIGTERM, SIGUSR1, SIGUSR2
#include <stdbool.h>   // for bool
#include <stdint.h>    // for int64_t
#include <stdlib.h>    // for EXIT_FAILURE, setenv
#include <string.h>    // for memcmp, memcpy, memset

#include "../env_settings.h"      // for GetIntSetting, GetExecutablePathSet...
#include "../io_util.h"           // for ClosePair, PipeCloexec, RetryPoll
#include "../logging.h"           // for Log, LogErrno
#include "../saver_child.h"       // for KillAllSaverChildren, MAX_SAVERS
#include "../signal_pipe.h"       // for SignalPipe, SignalPipeInit
#include "../time_util.h"         // for GetMonotonicTimeMs, SleepMs
#include "../util.h"              // for ARRAY_LEN
#include "../wm_properties.h"     // for SetWMProperties
#include "../xscreensaver_api.h"  // for ReadWindowID
#include "monitors.h"             // for IsMonitorChangeEvent, Monitor, Sele...

#define MAX_MONITORS MAX_SAVERS
#define MAX_X_EVENTS_PER_TICK 128
#define FAILED_SAVER_RESTART_DELAY_MS 1000

struct SaverMultiplexState {
  int argc;
  char **argv;
  Display *display;
  Window parent_window;
  int x11_fd;
  struct SignalPipe signal_pipe;
  const char *saver_executable;
  Monitor monitors[MAX_MONITORS];
  size_t num_monitors;
  Window windows[MAX_MONITORS];
  int terminate_signal;
  bool have_pending_x_events;
  bool child_state_dirty;
  int64_t next_child_watch_ms;
};

static volatile sig_atomic_t g_reset_requested = 0;
static volatile sig_atomic_t g_auth_open_requested = 0;
static volatile sig_atomic_t g_terminate_signal = 0;
static volatile sig_atomic_t g_child_state_dirty = 0;

static void HandleSIGUSR1(int unused_signo) {
  (void)unused_signo;
  g_reset_requested = 1;
  SignalPipeNotifyFromHandler();
}

static void HandleSIGUSR2(int unused_signo) {
  (void)unused_signo;
  g_auth_open_requested = 1;
  SignalPipeNotifyFromHandler();
}

static void HandleSIGTERM(int signo) {
  g_terminate_signal = signo;
  SignalPipeNotifyFromHandler();
}

static void HandleSIGCHLD(int unused_signo) {
  (void)unused_signo;
  g_child_state_dirty = 1;
  SignalPipeNotifyFromHandler();
}

static void InitState(struct SaverMultiplexState *state, int argc,
                      char **argv) {
  memset(state, 0, sizeof(*state));
  state->argc = argc;
  state->argv = argv;
  state->parent_window = None;
  state->signal_pipe.fds[0] = -1;
  state->signal_pipe.fds[1] = -1;
  for (size_t i = 0; i < ARRAY_LEN(state->windows); ++i) {
    state->windows[i] = None;
  }
}

static int InstallSignalHandlers(void) {
  struct sigaction sa = {.sa_flags = 0, .sa_handler = HandleSIGUSR1};
  sigemptyset(&sa.sa_mask);
  if (sigaction(SIGUSR1, &sa, NULL) != 0) {
    LogErrno("sigaction(SIGUSR1)");
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

  sa.sa_flags = SA_NOCLDSTOP;
  sa.sa_handler = HandleSIGCHLD;
  if (sigaction(SIGCHLD, &sa, NULL) != 0) {
    LogErrno("sigaction(SIGCHLD)");
    return 0;
  }

  return 1;
}

static void MarkChildStateDirty(struct SaverMultiplexState *state) {
  state->child_state_dirty = true;
}

static int HandleSignalFlags(struct SaverMultiplexState *state,
                             bool *watch_savers) {
  if (g_terminate_signal != 0) {
    state->terminate_signal = g_terminate_signal;
    g_terminate_signal = 0;
    return 1;
  }
  if (g_reset_requested) {
    g_reset_requested = 0;
    KillAllSaverChildren(SIGUSR1);
    MarkChildStateDirty(state);
    *watch_savers = true;
  }
  if (g_auth_open_requested) {
    g_auth_open_requested = 0;
    KillAllSaverChildren(SIGUSR2);
    MarkChildStateDirty(state);
    *watch_savers = true;
  }
  return 0;
}

static void HandleChildSignalFlag(struct SaverMultiplexState *state) {
  if (g_child_state_dirty) {
    g_child_state_dirty = 0;
    MarkChildStateDirty(state);
  }
}

static void WatchSavers(const struct SaverMultiplexState *state) {
  for (size_t i = 0; i < state->num_monitors; ++i) {
    if (state->windows[i] == None) {
      continue;
    }
    WatchSaverChild(state->display, state->windows[i], (int)i,
                    state->saver_executable, 1);
  }
}

static void NoteChildWatchFinished(struct SaverMultiplexState *state) {
  int64_t now_ms = 0;

  state->child_state_dirty = false;
  if (GetMonotonicTimeMs(&now_ms) != 0) {
    state->next_child_watch_ms = 0;
    return;
  }
  state->next_child_watch_ms = now_ms + FAILED_SAVER_RESTART_DELAY_MS;
}

static int ChildWatchTimeoutMs(const struct SaverMultiplexState *state) {
  int64_t now_ms = 0;

  if (!state->child_state_dirty) {
    return -1;
  }
  if (state->next_child_watch_ms == 0 || GetMonotonicTimeMs(&now_ms) != 0) {
    return 0;
  }
  if (now_ms >= state->next_child_watch_ms) {
    return 0;
  }
  return (int)(state->next_child_watch_ms - now_ms);
}

static int PollTimeoutMs(const struct SaverMultiplexState *state) {
  int child_watch_timeout_ms = ChildWatchTimeoutMs(state);

  if (state->have_pending_x_events) {
    return 0;
  }
  return child_watch_timeout_ms;
}

static void WatchSaversAfterWakeup(struct SaverMultiplexState *state) {
  WatchSavers(state);
  NoteChildWatchFinished(state);
}

static void StopSavers(struct SaverMultiplexState *state) {
  for (size_t i = 0; i < ARRAY_LEN(state->windows); ++i) {
    if (state->windows[i] == None) {
      continue;
    }
    WatchSaverChild(state->display, state->windows[i], (int)i,
                    state->saver_executable, 0);
    XDestroyWindow(state->display, state->windows[i]);
    state->windows[i] = None;
  }
}

static void SpawnSavers(struct SaverMultiplexState *state) {
  for (size_t i = 0; i < state->num_monitors; ++i) {
    state->windows[i] =
        XCreateWindow(state->display, state->parent_window,
                      state->monitors[i].x, state->monitors[i].y,
                      state->monitors[i].width, state->monitors[i].height, 0,
                      CopyFromParent, InputOutput, CopyFromParent, 0, NULL);
    SetWMProperties(state->display, state->windows[i], "xsecurelock",
                    "saver_multiplex_screen", state->argc, state->argv);
    XMapRaised(state->display, state->windows[i]);
  }

  XFlush(state->display);
  WatchSavers(state);
}

static int LoadMonitorSnapshot(struct SaverMultiplexState *state,
                               Monitor monitors[MAX_MONITORS],
                               size_t *num_monitors) {
  memset(monitors, 0, sizeof(*monitors) * MAX_MONITORS);
  *num_monitors =
      GetMonitors(state->display, state->parent_window, monitors, MAX_MONITORS);
  return *num_monitors != 0;
}

static void ReloadMonitorsIfChanged(struct SaverMultiplexState *state) {
  Monitor new_monitors[MAX_MONITORS];
  size_t new_num_monitors = 0;

  if (!LoadMonitorSnapshot(state, new_monitors, &new_num_monitors)) {
    Log("Could not refresh monitors; keeping current layout");
    return;
  }

  if (new_num_monitors == state->num_monitors &&
      memcmp(new_monitors, state->monitors, sizeof(new_monitors)) == 0) {
    return;
  }

  StopSavers(state);
  memcpy(state->monitors, new_monitors, sizeof(new_monitors));
  state->num_monitors = new_num_monitors;
  SpawnSavers(state);
}

static void ProcessOneXEvent(struct SaverMultiplexState *state,
                             const XEvent *event) {
  if (IsMonitorChangeEvent(state->display, event->type)) {
    ReloadMonitorsIfChanged(state);
  }
}

static void ProcessPendingXEvents(struct SaverMultiplexState *state) {
  XEvent event;
  int drained = 0;

  while (drained < MAX_X_EVENTS_PER_TICK && XPending(state->display) > 0) {
    XNextEvent(state->display, &event);
    ++drained;
    ProcessOneXEvent(state, &event);
  }

  state->have_pending_x_events = XPending(state->display) > 0;
}

/*! \brief The main program.
 *
 * Usage: XSCREENSAVER_WINDOW=window_id ./saver_multiplex
 *
 * Spawns spearate saver subprocesses, one on each screen.
 */
int main(int argc, char **argv) {
  struct SaverMultiplexState state;
  int status = EXIT_FAILURE;

  InitState(&state, argc, argv);

  if (GetBoolSetting("XSECURELOCK_INSIDE_SAVER_MULTIPLEX", 0)) {
    Log("Starting saver_multiplex inside saver_multiplex?!?");
    (void)SleepMs(60000);
    goto done;
  }
  setenv("XSECURELOCK_INSIDE_SAVER_MULTIPLEX", "1", 1);

  state.display = XOpenDisplay(NULL);
  if (state.display == NULL) {
    Log("Could not connect to $DISPLAY");
    goto done;
  }
  state.x11_fd = ConnectionNumber(state.display);

  state.parent_window = ReadWindowID();
  if (state.parent_window == None) {
    Log("Invalid/no parent ID in XSCREENSAVER_WINDOW");
    goto done;
  }

  state.saver_executable =
      GetExecutablePathSetting("XSECURELOCK_SAVER", SAVER_EXECUTABLE, 0);

  SelectMonitorChangeEvents(state.display, state.parent_window);
  if (!LoadMonitorSnapshot(&state, state.monitors, &state.num_monitors)) {
    Log("Could not determine monitor layout");
    goto done;
  }

  if (SignalPipeInit(&state.signal_pipe) != 0) {
    LogErrno("SignalPipeInit");
    goto done;
  }
  if (!SignalPipeSetWriteFdForHandler(state.signal_pipe.fds[1])) {
    LogErrno("SignalPipeSetWriteFdForHandler");
    goto done;
  }

  if (!InstallSignalHandlers()) {
    goto done;
  }

  SpawnSavers(&state);
  status = 0;

  for (;;) {
    bool watch_savers = false;
    struct pollfd fds[2] = {
        {.fd = state.x11_fd, .events = POLLIN | POLLHUP, .revents = 0},
        {.fd = state.signal_pipe.fds[0],
         .events = POLLIN | POLLHUP,
         .revents = 0},
    };
    int ready = RetryPoll(fds, ARRAY_LEN(fds), PollTimeoutMs(&state));
    if (ready < 0) {
      LogErrno("poll");
    }

    if (fds[1].revents & (POLLIN | POLLHUP)) {
      SignalPipeDrain(state.signal_pipe.fds[0], "signal pipe");
    }
    HandleChildSignalFlag(&state);
    if (HandleSignalFlags(&state, &watch_savers)) {
      goto done;
    }

    ProcessPendingXEvents(&state);
    if (watch_savers || ChildWatchTimeoutMs(&state) == 0) {
      WatchSaversAfterWakeup(&state);
    }
  }

done:
  if (state.display != NULL) {
    if (state.terminate_signal != 0) {
      KillAllSaverChildren(state.terminate_signal);
    }
    StopSavers(&state);
    XCloseDisplay(state.display);
  }
  SignalPipeClose(&state.signal_pipe);

  if (state.terminate_signal != 0) {
    struct sigaction sa = {.sa_flags = 0, .sa_handler = SIG_DFL};
    sigemptyset(&sa.sa_mask);
    (void)sigaction(state.terminate_signal, &sa, NULL);
    (void)raise(state.terminate_signal);
  }

  return status;
}
