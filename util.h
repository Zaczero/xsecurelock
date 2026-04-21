// util header file
//
// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stddef.h>      // for size_t
#include <stdint.h>      // for int64_t
#include <poll.h>        // for pollfd, nfds_t
#include <sys/types.h>   // for ssize_t

// Use the libc implementation when available; otherwise util.c provides a
// local fallback with the same signature.
void explicit_bzero(void *s, size_t len);

int GetMonotonicTimeMs(int64_t *time_ms);
int PipeCloexec(int fds[2]);
ssize_t RetryRead(int fd, void *buf, size_t len);
ssize_t RetryWrite(int fd, const void *buf, size_t len);
int RetryPoll(struct pollfd *fds, nfds_t nfds, int timeout_ms);
