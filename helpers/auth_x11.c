/*
 * Copyright 2014 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"

#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "../buf_util.h"
#include "../io_util.h"
#include "../logging.h"
#include "../wait_pgrp.h"
#include "auth_draw.h"
#include "auth_prompt.h"
#include "auth_ui.h"
#include "auth_ui_config.h"
#include "auth_ui_resources.h"
#include "auth_windows.h"
#include "authproto.h"

static void SeedPromptRngFromClock(struct AuthUiContext *ctx) {
  struct timeval tv;

  gettimeofday(&tv, NULL);
  SeedPromptRng(&ctx->runtime.prompt_rng, (uint32_t)tv.tv_sec ^
                                              (uint32_t)tv.tv_usec ^
                                              (uint32_t)getpid());
}

static void InitializeBurnInOffsets(struct AuthUiContext *ctx) {
  if (ctx->config.burnin_mitigation_max_offset <= 0) {
    return;
  }

  ctx->runtime.x_offset = RandomRangeInclusive(
      &ctx->runtime.prompt_rng, -ctx->config.burnin_mitigation_max_offset,
      ctx->config.burnin_mitigation_max_offset);
  ctx->runtime.y_offset = RandomRangeInclusive(
      &ctx->runtime.prompt_rng, -ctx->config.burnin_mitigation_max_offset,
      ctx->config.burnin_mitigation_max_offset);
}

static void TerminateAuthproto(pid_t childpid) {
  if (kill(childpid, SIGTERM) != 0 && errno != ESRCH) {
    LogErrno("kill");
  }
}

static int ShowProcessingMessage(struct AuthUiContext *ctx) {
  return AuthDisplayMessage(ctx, "Processing...", "", false);
}

static int HandleStaticAuthMessage(struct AuthUiContext *ctx, pid_t childpid,
                                   int *already_killed, const char *title,
                                   const char *message, bool warning,
                                   int request_fd) {
  switch (AuthWaitStaticMessage(ctx, title, message, warning, request_fd)) {
    case STATIC_MESSAGE_RESULT_ADVANCE:
      return 1;
    case STATIC_MESSAGE_RESULT_CANCELLED:
      TerminateAuthproto(childpid);
      *already_killed = 1;
      return 0;
    case STATIC_MESSAGE_RESULT_FAILED:
      return 0;
  }
  return 0;
}

static int HandlePromptAuthMessage(struct AuthUiContext *ctx,
                                   const char *message, bool echo,
                                   int response_fd, char response_type) {
  switch (
      AuthRunPromptSession(ctx, message, echo, response_fd, response_type)) {
    case PROMPT_SESSION_RESULT_SUBMITTED:
      return ShowProcessingMessage(ctx);
    case PROMPT_SESSION_RESULT_CANCELLED:
      if (!WritePacketBytes(response_fd, PTYPE_RESPONSE_CANCELLED, "", 0)) {
        return 0;
      }
      return ShowProcessingMessage(ctx);
    case PROMPT_SESSION_RESULT_FAILED:
      return 0;
  }
  return 0;
}

static int DuplicateAwayFromStdio(int *fd) {
  if (fd == NULL) {
    errno = EINVAL;
    return 0;
  }
  if (*fd != STDIN_FILENO && *fd != STDOUT_FILENO) {
    return 1;
  }

  int dupfd = fcntl(*fd, F_DUPFD, STDERR_FILENO + 1);
  if (dupfd < 0) {
    return 0;
  }
  if (CloseIfValid(fd) != 0) {
    close(dupfd);
    return 0;
  }
  *fd = dupfd;
  return 1;
}

static int PrepareAuthprotoChildFds(int requestfd[2], int responsefd[2]) {
  if (CloseIfValid(&requestfd[0]) != 0 || CloseIfValid(&responsefd[1]) != 0) {
    return 0;
  }
  if (requestfd[1] == STDIN_FILENO && !DuplicateAwayFromStdio(&requestfd[1])) {
    return 0;
  }
  if (responsefd[0] == STDOUT_FILENO &&
      !DuplicateAwayFromStdio(&responsefd[0])) {
    return 0;
  }
  if (MoveFdTo(&responsefd[0], STDIN_FILENO) != 0) {
    return 0;
  }
  if (MoveFdTo(&requestfd[1], STDOUT_FILENO) != 0) {
    return 0;
  }
  return 1;
}

static int Authenticate(struct AuthUiContext *ctx) {
  int status = 1;
  int requestfd[2] = {-1, -1};
  int responsefd[2] = {-1, -1};
  pid_t childpid = 0;
  int already_killed = 0;

  if (PipeCloexec(requestfd) != 0) {
    LogErrno("PipeCloexec");
    goto done;
  }
  if (PipeCloexec(responsefd) != 0) {
    LogErrno("PipeCloexec");
    goto done;
  }

  childpid = ForkWithoutSigHandlers();
  if (childpid == -1) {
    LogErrno("fork");
    childpid = 0;
    goto done;
  }
  if (childpid == 0) {
    if (!PrepareAuthprotoChildFds(requestfd, responsefd)) {
      LogErrno("PrepareAuthprotoChildFds");
      _exit(EXIT_FAILURE);
    }
    {
      const char *args[2] = {ctx->config.authproto_executable, NULL};
      ExecvHelperOrExit(ctx->config.authproto_executable, args);
    }
  }

  (void)CloseIfValid(&requestfd[1]);
  (void)CloseIfValid(&responsefd[0]);

  for (;;) {
    char *message = NULL;
    char type = ReadPacket(requestfd[0], &message, 1);
    int keep_running = 1;

    if (type == 0) {
      ClearFreeString(&message);
      break;
    }

    switch (type) {
      case PTYPE_INFO_MESSAGE:
        keep_running =
            HandleStaticAuthMessage(ctx, childpid, &already_killed, "PAM says",
                                    message, false, requestfd[0]);
        break;
      case PTYPE_ERROR_MESSAGE:
        keep_running =
            HandleStaticAuthMessage(ctx, childpid, &already_killed, "Error",
                                    message, true, requestfd[0]);
        break;
      case PTYPE_PROMPT_LIKE_USERNAME:
        keep_running = HandlePromptAuthMessage(
            ctx, message, true, responsefd[1], PTYPE_RESPONSE_LIKE_USERNAME);
        break;
      case PTYPE_PROMPT_LIKE_PASSWORD:
        keep_running = HandlePromptAuthMessage(
            ctx, message, false, responsefd[1], PTYPE_RESPONSE_LIKE_PASSWORD);
        break;
      default:
        Log("Unknown message type %02x", (unsigned int)(unsigned char)type);
        keep_running = 0;
        break;
    }

    ClearFreeString(&message);
    if (!keep_running) {
      break;
    }
  }

done:
  (void)ClosePair(requestfd);
  (void)ClosePair(responsefd);
  if (childpid != 0) {
    int child_status = 1;
    if (!WaitProc("authproto", &childpid, 1, already_killed, &child_status)) {
      Log("WaitPgrp returned false but we were blocking");
      abort();
    }
    if (child_status == 0) {
      AuthPlaySound(ctx, AUTH_SOUND_SUCCESS);
    }
    status = child_status != 0;
  }
  return status;
}

int main(int argc, char **argv) {
  struct AuthUiContext ctx;
  int status = EXIT_FAILURE;

  AuthUiContextInit(&ctx, argc, argv);

  (void)setlocale(LC_CTYPE, "");
  (void)setlocale(LC_TIME, "");

  SeedPromptRngFromClock(&ctx);
  if (!AuthUiConfigLoad(&ctx.config)) {
    goto done;
  }

  InitializeBurnInOffsets(&ctx);
  if (!AuthUiResourcesInit(&ctx.resources, &ctx.config)) {
    goto done;
  }

  InitWaitPgrp();
  status = Authenticate(&ctx);

done:
  AuthWindowsDestroy(&ctx, 0);
  AuthUiResourcesCleanup(&ctx.resources);
  return status;
}
