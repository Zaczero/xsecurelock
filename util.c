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
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#ifndef HAVE_EXPLICIT_BZERO
#define HAVE_EXPLICIT_BZERO 0
#endif

#ifndef FORCE_EXPLICIT_BZERO_FALLBACK
#define FORCE_EXPLICIT_BZERO_FALLBACK 0
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

static void ComputeRemainingTimeout(const struct timeval *timeout,
                                    const struct timeval *start,
                                    struct timeval *remaining) {
  *remaining = *timeout;

  struct timeval now;
  if (gettimeofday(&now, NULL) != 0) {
    return;
  }

  remaining->tv_sec = timeout->tv_sec - (now.tv_sec - start->tv_sec);
  remaining->tv_usec = timeout->tv_usec - (now.tv_usec - start->tv_usec);
  if (remaining->tv_usec < 0) {
    remaining->tv_usec += 1000000;
    --remaining->tv_sec;
  }
  if (remaining->tv_sec < 0) {
    remaining->tv_sec = 0;
    remaining->tv_usec = 0;
  }
}

int RetrySelect(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
                const struct timeval *timeout) {
  fd_set saved_readfds;
  fd_set saved_writefds;
  fd_set saved_exceptfds;
  int have_timeout = (timeout != NULL);
  int have_start_time = 0;
  struct timeval start;

  if (readfds != NULL) {
    saved_readfds = *readfds;
  }
  if (writefds != NULL) {
    saved_writefds = *writefds;
  }
  if (exceptfds != NULL) {
    saved_exceptfds = *exceptfds;
  }
  if (have_timeout && gettimeofday(&start, NULL) == 0) {
    have_start_time = 1;
  }

  for (;;) {
    fd_set local_readfds;
    fd_set local_writefds;
    fd_set local_exceptfds;
    fd_set *readfds_ptr = NULL;
    fd_set *writefds_ptr = NULL;
    fd_set *exceptfds_ptr = NULL;
    struct timeval local_timeout;
    struct timeval *timeout_ptr = NULL;

    if (readfds != NULL) {
      local_readfds = saved_readfds;
      readfds_ptr = &local_readfds;
    }
    if (writefds != NULL) {
      local_writefds = saved_writefds;
      writefds_ptr = &local_writefds;
    }
    if (exceptfds != NULL) {
      local_exceptfds = saved_exceptfds;
      exceptfds_ptr = &local_exceptfds;
    }
    if (have_timeout) {
      if (have_start_time) {
        ComputeRemainingTimeout(timeout, &start, &local_timeout);
      } else {
        local_timeout = *timeout;
      }
      timeout_ptr = &local_timeout;
    }

    int ret = select(nfds, readfds_ptr, writefds_ptr, exceptfds_ptr,
                     timeout_ptr);
    if (ret >= 0) {
      if (readfds != NULL) {
        *readfds = local_readfds;
      }
      if (writefds != NULL) {
        *writefds = local_writefds;
      }
      if (exceptfds != NULL) {
        *exceptfds = local_exceptfds;
      }
      return ret;
    }
    if (errno != EINTR) {
      return ret;
    }
  }
}
