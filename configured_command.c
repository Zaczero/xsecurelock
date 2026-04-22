/*
Copyright 2018 Google Inc. All rights reserved.

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

#include "configured_command.h"

#include <errno.h>      // for errno, EINTR
#include <signal.h>     // for sigaction, sigemptyset, SIGPIPE, SIGUSR2
#include <stdio.h>      // for snprintf
#include <stdlib.h>     // for EXIT_FAILURE, EXIT_SUCCESS
#include <sys/wait.h>   // for waitpid, WEXITSTATUS, WIFEXITED, WIFSIGNALED
#include <unistd.h>     // for _exit, execl, fork, pid_t, setsid

#include "env_settings.h"
#include "logging.h"
#include "wait_pgrp.h"

static void ExecShellOrExit(const char *label, const char *command) {
  execl("/bin/sh", "sh", "-c", command, (char *)NULL);
  LogErrno("execl /bin/sh -c for %s", label);
  _exit(EXIT_FAILURE);
}

static void ResetSignalToDefault(int signo, const char *label) {
  struct sigaction sa = {.sa_flags = 0, .sa_handler = SIG_DFL};
  sigemptyset(&sa.sa_mask);
  if (sigaction(signo, &sa, NULL) != 0) {
    LogErrno("sigaction(%d) for %s", signo, label);
    _exit(EXIT_FAILURE);
  }
}

static int WaitForChild(const char *label, pid_t childpid, int *status) {
  for (;;) {
    pid_t gotpid = waitpid(childpid, status, 0);
    if (gotpid == childpid) {
      return 1;
    }
    if (gotpid < 0 && errno == EINTR) {
      continue;
    }
    LogErrno("waitpid for %s", label);
    return 0;
  }
}

static int LogCommandStatus(const char *label, int status) {
  if (WIFSIGNALED(status)) {
    Log("%s terminated by signal %d", label, WTERMSIG(status));
    return 0;
  }
  if (!WIFEXITED(status)) {
    Log("%s ended unexpectedly", label);
    return 0;
  }
  if (WEXITSTATUS(status) != 0) {
    Log("%s exited with status %d", label, WEXITSTATUS(status));
    return 0;
  }
  return 1;
}

int FormatKeyCommandEnvName(char *dst, size_t dst_size, const char *keyname) {
  int len = snprintf(dst, dst_size, "XSECURELOCK_KEY_%s_COMMAND", keyname);
  if (len <= 0 || (size_t)len >= dst_size) {
    Log("Wow, pretty long keysym names you got there");
    return 0;
  }
  return 1;
}

int RunShellCommandValue(const char *label, const char *command, int background) {
  if (command == NULL || *command == '\0') {
    return 1;
  }

  pid_t childpid = ForkWithoutSigHandlers();
  if (childpid == -1) {
    LogErrno("fork for %s", label);
    return 0;
  }

  if (childpid == 0) {
    ResetSignalToDefault(SIGPIPE, label);
    ResetSignalToDefault(SIGUSR2, label);
    if (background) {
      if (setsid() == (pid_t)-1) {
        LogErrno("setsid for %s", label);
        _exit(EXIT_FAILURE);
      }

      pid_t grandchildpid = fork();
      if (grandchildpid == -1) {
        LogErrno("fork for %s", label);
        _exit(EXIT_FAILURE);
      }
      if (grandchildpid != 0) {
        _exit(EXIT_SUCCESS);
      }
    }

    ExecShellOrExit(label, command);
  }

  int status = 0;
  if (!WaitForChild(label, childpid, &status)) {
    return 0;
  }
  return LogCommandStatus(label, status);
}

int RunShellCommandFromEnv(const char *env_name, int background) {
  const char *command = GetStringSetting(env_name, "");
  return RunShellCommandValue(env_name, command, background);
}
