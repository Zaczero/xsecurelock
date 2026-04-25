#include "config.h"

#include "auth_prompt.h"

#include <poll.h>
#include <stdint.h>

#include "../io_util.h"
#include "../logging.h"
#include "../mlock_page.h"
#include "../time_util.h"
#include "auth_draw.h"
#include "authproto.h"
#include "prompt_state.h"
#include "xkb.h"

#define BLINK_INTERVAL_MS 250
#define MAX_MONITOR_EVENTS_PER_TICK 128

static const char kCursor[] = "_";

enum AuthActivityResult {
  AUTH_ACTIVITY_REDRAW,
  AUTH_ACTIVITY_INPUT_READY,
  AUTH_ACTIVITY_EXTRA_FD_READY,
  AUTH_ACTIVITY_TIMEOUT,
  AUTH_ACTIVITY_FAILED,
};

static void WaitForKeypress(int seconds) {
  struct pollfd pfd = {.fd = 0, .events = POLLIN | POLLHUP, .revents = 0};
  (void)RetryPoll(&pfd, 1, seconds * 1000);
}

static void DrainMonitorChangeEvents(struct AuthUiContext *ctx) {
  XEvent ev;
  int drained = 0;

  while (drained < MAX_MONITOR_EVENTS_PER_TICK &&
         XPending(ctx->resources.display) > 0) {
    XNextEvent(ctx->resources.display, &ev);
    if (IsMonitorChangeEvent(ctx->resources.display, ev.type)) {
      ctx->windows.dirty = true;
    }
    ++drained;
  }
  if (XPending(ctx->resources.display) > 0) {
    ctx->windows.dirty = true;
  }
}

static enum AuthActivityResult WaitForAuthActivity(struct AuthUiContext *ctx,
                                                   int extra_read_fd,
                                                   int64_t *deadline_ms,
                                                   bool poll_only,
                                                   char *inputbuf) {
  struct pollfd fds[2] = {
      {.fd = 0, .events = POLLIN | POLLHUP, .revents = 0},
      {.fd = -1, .events = POLLIN | POLLHUP, .revents = 0},
  };
  nfds_t nfds = 1;
  int64_t now_ms = 0;

  if (extra_read_fd >= 0) {
    fds[1].fd = extra_read_fd;
    nfds = 2;
  }

  int ready = RetryPoll(fds, nfds, poll_only ? 0 : BLINK_INTERVAL_MS);
  if (ready < 0) {
    LogErrno("poll");
    return AUTH_ACTIVITY_FAILED;
  }
  if (GetMonotonicTimeMs(&now_ms) != 0) {
    LogErrno("clock_gettime");
    return AUTH_ACTIVITY_FAILED;
  }
  if (now_ms >= *deadline_ms) {
    return AUTH_ACTIVITY_TIMEOUT;
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

  {
    ssize_t nread = RetryRead(0, inputbuf, 1);
    if (nread < 0) {
      LogErrno("read");
      return AUTH_ACTIVITY_FAILED;
    }
    if (nread == 0) {
      Log("EOF on password input - bailing out");
      return AUTH_ACTIVITY_FAILED;
    }
  }

  *deadline_ms = now_ms + ((int64_t)ctx->config.prompt_timeout * 1000);
  return AUTH_ACTIVITY_INPUT_READY;
}

static int ShowPromptStorageWarning(struct AuthUiContext *ctx,
                                    const char *message) {
  if (!AuthDisplayMessage(ctx, "Error", message, true)) {
    return 0;
  }
  WaitForKeypress(1);
  return 1;
}

static int RenderPromptFrame(struct AuthUiContext *ctx,
                             const char *prompt_title,
                             const struct PromptState *state, bool echo,
                             int blink_state) {
  char displaybuf[PROMPT_DISPLAY_BUFFER_SIZE];
  size_t displaylen = 0;

  if (RenderPromptDisplay(ctx->config.prompt_display_mode, state, echo,
                          blink_state, *kCursor, displaybuf, sizeof(displaybuf),
                          &displaylen) == 0) {
    return AuthDisplayMessage(ctx, prompt_title, displaybuf, false);
  }
  if (echo) {
    Log("Prompt rendering failed in echo mode");
    return 0;
  }

  Log("Prompt rendering failed; falling back to cursor display");
  if (RenderPromptDisplay(PROMPT_DISPLAY_MODE_CURSOR, state, false, blink_state,
                          *kCursor, displaybuf, sizeof(displaybuf),
                          &displaylen) != 0) {
    Log("Cursor prompt rendering fallback failed");
    return 0;
  }
  return AuthDisplayMessage(ctx, prompt_title, displaybuf, false);
}

static int HandlePromptInputByte(struct AuthUiContext *ctx,
                                 struct PromptState *state, char input_byte,
                                 int response_fd, char response_type,
                                 enum PromptSessionResult *result) {
  size_t marker_count =
      PromptDisplayMarkerCount(ctx->config.prompt_display_mode);
  size_t min_change = PromptDisplayMinChange(ctx->config.prompt_display_mode);

  switch (input_byte) {
    case '\b':
    case '\177':
      PromptStateDeleteLastGlyph(state);
      PromptStateBumpDisplayMarker(state, &ctx->runtime.prompt_rng,
                                   marker_count, min_change);
      return 0;
    case '\001':
    case '\025':
      PromptStateClear(state);
      PromptStateBumpDisplayMarker(state, &ctx->runtime.prompt_rng,
                                   marker_count, min_change);
      return 0;
    case '\023':
      SwitchToNextXkbLayout(ctx->resources.display,
                            ctx->resources.have_xkb_ext);
      return 0;
    case 0:
      *result = PROMPT_SESSION_RESULT_FAILED;
      return 1;
    case '\033':
      *result = PROMPT_SESSION_RESULT_CANCELLED;
      return 1;
    case '\r':
    case '\n':
      if (!WritePacketBytes(response_fd, response_type, state->password,
                            state->password_length)) {
        *result = PROMPT_SESSION_RESULT_FAILED;
      } else {
        *result = PROMPT_SESSION_RESULT_SUBMITTED;
      }
      return 1;
    default:
      if (input_byte >= '\000' && input_byte <= '\037') {
        return 0;
      }
      if (!PromptStateAppendByte(state, input_byte)) {
        Log("Password entered is too long - bailing out");
        *result = PROMPT_SESSION_RESULT_FAILED;
        return 1;
      }
      PromptStateBumpDisplayMarker(state, &ctx->runtime.prompt_rng,
                                   marker_count, min_change);
      return 0;
  }
}

enum StaticMessageResult AuthWaitStaticMessage(struct AuthUiContext *ctx,
                                               const char *title,
                                               const char *message,
                                               bool warning,
                                               int extra_read_fd) {
  bool played_sound = false;
  char inputbuf = 0;
  int64_t deadline_ms = 0;

  if (GetMonotonicTimeMs(&deadline_ms) != 0) {
    LogErrno("clock_gettime");
    return STATIC_MESSAGE_RESULT_FAILED;
  }
  deadline_ms += (int64_t)ctx->config.prompt_timeout * 1000;

  for (;;) {
    if (!AuthDisplayMessage(ctx, title, message, warning)) {
      return STATIC_MESSAGE_RESULT_FAILED;
    }
    if (!played_sound) {
      AuthPlaySound(ctx, warning ? AUTH_SOUND_ERROR : AUTH_SOUND_INFO);
      played_sound = true;
    }

    switch (WaitForAuthActivity(ctx, extra_read_fd, &deadline_ms, false,
                                &inputbuf)) {
      case AUTH_ACTIVITY_REDRAW:
        DrainMonitorChangeEvents(ctx);
        break;
      case AUTH_ACTIVITY_INPUT_READY:
        DrainMonitorChangeEvents(ctx);
        if (inputbuf == '\033') {
          return STATIC_MESSAGE_RESULT_CANCELLED;
        }
        break;
      case AUTH_ACTIVITY_EXTRA_FD_READY:
        DrainMonitorChangeEvents(ctx);
        return STATIC_MESSAGE_RESULT_ADVANCE;
      case AUTH_ACTIVITY_TIMEOUT:
        Log("AUTH_TIMEOUT hit");
        return STATIC_MESSAGE_RESULT_CANCELLED;
      case AUTH_ACTIVITY_FAILED:
        return STATIC_MESSAGE_RESULT_FAILED;
    }
  }
}

enum PromptSessionResult AuthRunPromptSession(struct AuthUiContext *ctx,
                                              const char *prompt_title,
                                              bool echo, int response_fd,
                                              char response_type) {
  struct PromptState state;
  char input_byte = 0;
  int blink_state = 0;
  bool played_sound = false;
  bool done = false;
  enum PromptSessionResult result = PROMPT_SESSION_RESULT_CANCELLED;
  int64_t deadline_ms = 0;

  PromptStateInit(&state);

  if (!echo && MLOCK_PAGE(&state, sizeof(state)) < 0) {
    LogErrno("mlock");
    if (!ShowPromptStorageWarning(ctx,
                                  "Password will not be stored securely.")) {
      result = PROMPT_SESSION_RESULT_FAILED;
      done = true;
    }
  }
  if (!done && GetMonotonicTimeMs(&deadline_ms) != 0) {
    LogErrno("clock_gettime");
    result = PROMPT_SESSION_RESULT_FAILED;
    done = true;
  }
  if (!done) {
    deadline_ms += (int64_t)ctx->config.prompt_timeout * 1000;
  }

  while (!done) {
    bool redraw_requested = false;
    bool poll_only = false;

    if (!RenderPromptFrame(ctx, prompt_title, &state, echo, blink_state)) {
      result = PROMPT_SESSION_RESULT_FAILED;
      break;
    }
    if (!played_sound) {
      AuthPlaySound(ctx, AUTH_SOUND_PROMPT);
      played_sound = true;
    }
    if (ctx->config.auth_cursor_blink) {
      blink_state = !blink_state;
    }

    while (!done && !redraw_requested) {
      switch (
          WaitForAuthActivity(ctx, -1, &deadline_ms, poll_only, &input_byte)) {
        case AUTH_ACTIVITY_REDRAW:
          redraw_requested = true;
          break;
        case AUTH_ACTIVITY_TIMEOUT:
          Log("AUTH_TIMEOUT hit");
          result = PROMPT_SESSION_RESULT_CANCELLED;
          done = true;
          break;
        case AUTH_ACTIVITY_FAILED:
          result = PROMPT_SESSION_RESULT_FAILED;
          done = true;
          break;
        case AUTH_ACTIVITY_EXTRA_FD_READY:
          Log("Unexpected authproto readiness while prompting");
          result = PROMPT_SESSION_RESULT_FAILED;
          done = true;
          break;
        case AUTH_ACTIVITY_INPUT_READY:
          poll_only = true;
          blink_state = 0;
          done = HandlePromptInputByte(ctx, &state, input_byte, response_fd,
                                       response_type, &result) != 0;
          break;
      }
    }

    if (!done) {
      DrainMonitorChangeEvents(ctx);
    }
  }

  PromptStateWipe(&state);
  return result;
}
