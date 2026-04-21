/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * An earlier version of this file was originally released into the public
 * domain by its authors. It has been modified to make the code compile and
 * link as part of the Google Authenticator project. These changes are
 * copyrighted by Google Inc. and released under the Apache License,
 * Version 2.0.
 *
 * The previous authors' terms are included below:
 */

/*****************************************************************************
 *
 * File:    util.c
 *
 * Purpose: Collection of cross file utility functions.
 *
 * This code is in the public domain
 *
 *****************************************************************************
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifndef HAVE_EXPLICIT_BZERO
#define HAVE_EXPLICIT_BZERO 0
#endif

#ifndef FORCE_EXPLICIT_BZERO_FALLBACK
#define FORCE_EXPLICIT_BZERO_FALLBACK 0
#endif

#ifndef HAVE_CLOCK_GETTIME
#define HAVE_CLOCK_GETTIME 0
#endif

#ifndef HAVE_PIPE2
#define HAVE_PIPE2 0
#endif

#ifndef FORCE_PIPE2_FALLBACK
#define FORCE_PIPE2_FALLBACK 0
#endif

#if HAVE_PIPE2 && !FORCE_PIPE2_FALLBACK && defined(O_CLOEXEC)
// With the repo's conservative feature-test macros, some libcs may hide the
// prototype for pipe2() even when the function itself is available.
int pipe2(int pipefd[2], int flags);
#endif

#if !HAVE_EXPLICIT_BZERO || FORCE_EXPLICIT_BZERO_FALLBACK
// Prefer libc explicit_bzero() when it exists. Otherwise, route memset()
// through a volatile function pointer so the wipe remains an observable call
// without relying on compiler-specific inline assembly.
static void *(*const volatile memset_impl)(void *, int, size_t) = memset;

void explicit_bzero(void *s, size_t len) {
  memset_impl(s, 0, len);
}
#endif

ssize_t RetryRead(int fd, void *buf, size_t len) {
  ssize_t ret;
  do {
    ret = read(fd, buf, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

ssize_t RetryWrite(int fd, const void *buf, size_t len) {
  ssize_t ret;
  do {
    ret = write(fd, buf, len);
  } while (ret < 0 && errno == EINTR);
  return ret;
}

int GetMonotonicTimeMs(int64_t *time_ms) {
  if (time_ms == NULL) {
    errno = EINVAL;
    return -1;
  }

#if HAVE_CLOCK_GETTIME && defined(CLOCK_MONOTONIC)
  {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
      *time_ms = (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
      return 0;
    }
  }
#endif

  {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
      return -1;
    }
    *time_ms = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
    return 0;
  }
}

static int SetCloexec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  if (flags < 0) {
    return -1;
  }
  return fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}

int PipeCloexec(int fds[2]) {
#if HAVE_PIPE2 && !FORCE_PIPE2_FALLBACK && defined(O_CLOEXEC)
  if (pipe2(fds, O_CLOEXEC) == 0) {
    return 0;
  }
  if (errno != ENOSYS && errno != EINVAL) {
    return -1;
  }
#endif

  if (pipe(fds) != 0) {
    return -1;
  }
  if (SetCloexec(fds[0]) != 0 || SetCloexec(fds[1]) != 0) {
    int saved_errno = errno;
    close(fds[0]);
    close(fds[1]);
    errno = saved_errno;
    return -1;
  }
  return 0;
}

static int ComputeRemainingTimeoutMs(int timeout_ms, int64_t start_ms) {
  int64_t now_ms;
  if (GetMonotonicTimeMs(&now_ms) != 0) {
    return timeout_ms;
  }

  int64_t elapsed_ms = now_ms - start_ms;
  if (elapsed_ms >= timeout_ms) {
    return 0;
  }
  return timeout_ms - (int)elapsed_ms;
}

int RetryPoll(struct pollfd *fds, nfds_t nfds, int timeout_ms) {
  int have_timeout = (timeout_ms >= 0);
  int have_start_time = 0;
  int64_t start_ms = 0;

  if (have_timeout && GetMonotonicTimeMs(&start_ms) == 0) {
    have_start_time = 1;
  }

  for (;;) {
    for (nfds_t i = 0; i < nfds; ++i) {
      fds[i].revents = 0;
    }

    int local_timeout_ms = timeout_ms;
    if (have_timeout && have_start_time) {
      local_timeout_ms = ComputeRemainingTimeoutMs(timeout_ms, start_ms);
    }

    int ret = poll(fds, nfds, local_timeout_ms);
    if (ret >= 0) {
      return ret;
    }
    if (errno != EINTR) {
      return ret;
    }
  }
}
