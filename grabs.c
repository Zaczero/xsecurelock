#include "config.h"

#include "grabs.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <stdio.h>

#include "configured_command.h"
#include "logging.h"
#include "unmap_all.h"

#define ALL_POINTER_EVENTS                                                   \
  (ButtonPressMask | ButtonReleaseMask | EnterWindowMask | LeaveWindowMask | \
   PointerMotionMask | Button1MotionMask | Button2MotionMask |               \
   Button3MotionMask | Button4MotionMask | Button5MotionMask |               \
   ButtonMotionMask)

static void DebugDumpWindowInfo(const struct LockContext *ctx, Window w) {
  if (!ctx->config.debug_window_info) {
    return;
  }

  char buf[128] = "";
  int buflen = snprintf(buf, sizeof(buf),
                        "{ xwininfo -all -id %lu; xprop -id %lu; } >&2", w, w);
  if (buflen <= 0 || (size_t)buflen >= sizeof(buf)) {
    Log("Wow, pretty large integers you got there");
    return;
  }
  (void)RunShellCommandValue("debug window info", buf, 1);
}

void LockMaybeRaiseWindow(struct LockContext *ctx, Window w, int silent,
                          int force) {
  int need_raise = force;
  Window root = None;
  Window parent = None;
  Window *children = NULL;
  Window *siblings = NULL;
  unsigned int nchildren = 0;
  unsigned int nsiblings = 0;

  if (XQueryTree(ctx->runtime.display, w, &root, &parent, &children,
                 &nchildren)) {
    XFree(children);
    Window grandparent = None;
    if (!XQueryTree(ctx->runtime.display, parent, &root, &grandparent,
                    &siblings, &nsiblings)) {
      if (!silent) {
        Log("XQueryTree failed on the parent");
      }
      siblings = NULL;
      nsiblings = 0;
    }
  } else {
    if (!silent) {
      Log("XQueryTree failed on self");
    }
    siblings = NULL;
    nsiblings = 0;
  }
  if (nsiblings == 0) {
    if (!silent) {
      Log("No siblings found");
    }
  } else if (w == siblings[nsiblings - 1]) {
    if (force && !silent) {
      Log("MaybeRaiseWindow miss: something obscured my window %lu but I can't "
          "find it",
          w);
    }
  } else {
    if (!silent) {
      Log("MaybeRaiseWindow hit: window %lu was above my window %lu",
          siblings[nsiblings - 1], w);
      DebugDumpWindowInfo(ctx, siblings[nsiblings - 1]);
    }
    need_raise = 1;
  }
  XFree(siblings);
  if (need_raise) {
    XRaiseWindow(ctx->runtime.display, w);
  }
}

struct AcquireGrabsState {
  Display *display;
  Window root_window;
  Cursor cursor;
  int silent;
  const struct LockContext *ctx;
};

static int TryAcquireGrabs(Window w, void *state_voidp) {
  struct AcquireGrabsState *state = state_voidp;
  int ok = 1;

  if (XGrabPointer(state->display, state->root_window, False,
                   ALL_POINTER_EVENTS, GrabModeAsync, GrabModeAsync, None,
                   state->cursor, CurrentTime) != GrabSuccess) {
    if (!state->silent) {
      Log("Critical: cannot grab pointer");
    }
    ok = 0;
  }
  if (XGrabKeyboard(state->display, state->root_window, False, GrabModeAsync,
                    GrabModeAsync, CurrentTime) != GrabSuccess) {
    if (!state->silent) {
      Log("Critical: cannot grab keyboard");
    }
    ok = 0;
  }
  if (w != None) {
    Log("Unmapped window %lu to force grabbing, which %s", w,
        ok ? "succeeded" : "didn't help");
    if (ok) {
      DebugDumpWindowInfo(state->ctx, w);
    }
  }
  return ok;
}

int LockAcquireGrabs(struct LockContext *ctx, int silent, int force) {
  struct AcquireGrabsState grab_state = {
      .display = ctx->runtime.display,
      .root_window = ctx->windows.root_window,
      .cursor = ctx->windows.transparent_cursor,
      .silent = silent,
      .ctx = ctx,
  };

  if (!force) {
    return TryAcquireGrabs(None, &grab_state);
  }

  XGrabServer(ctx->runtime.display);
  UnmapAllWindowsState unmap_state;
  int ok = 0;
  if (InitUnmapAllWindowsState(
          &unmap_state, ctx->runtime.display, ctx->windows.root_window,
          ctx->windows.tracked_windows, ctx->windows.tracked_window_count,
          "xsecurelock", NULL, force > 1)) {
    Log("Trying to force grabbing by unmapping all windows. BAD HACK");
    ok = UnmapAllWindows(&unmap_state, TryAcquireGrabs, &grab_state);
    RemapAllWindows(&unmap_state);
  } else {
    Log("Found XSecureLock to be already running, not forcing");
    ok = TryAcquireGrabs(None, &grab_state);
  }
  ClearUnmapAllWindowsState(&unmap_state);
  XUngrabServer(ctx->runtime.display);
  XFlush(ctx->runtime.display);
  return ok;
}
