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

#include "authproto.h"

#include <stdio.h>   // for snprintf
#include <stdlib.h>  // for malloc, size_t
#include <string.h>  // for strlen
#include <unistd.h>  // for read, write, ssize_t

#include "../logging.h"     // for LogErrno, Log
#include "../mlock_page.h"  // for MLOCK_PAGE
#include "../util.h"        // for explicit_bzero

#define MAX_PACKET_LENGTH 0xFFFE
#define MAX_PACKET_LENGTH_DIGITS 5

static size_t WriteChars(int fd, const char *buf, size_t n) {
  size_t total = 0;
  while (total < n) {
    ssize_t got = RetryWrite(fd, buf + total, n - total);
    if (got < 0) {
      LogErrno("write");
      return 0;
    }
    if (got == 0) {
      Log("write: could not write anything, send buffer full");
      return 0;
    }
    if ((size_t)got > n - total) {
      Log("write: overlong write (should never happen)");
    }
    total += got;
  }
  return total;
}

void WritePacket(int fd, char type, const char *message) {
  size_t len_s = strlen(message);
  if (len_s >= 0xFFFF) {
    Log("overlong message, cannot write (hardcoded limit)");
    return;
  }
  int len = len_s;
  if (len < 0 || (size_t)len != len_s) {
    Log("overlong message, cannot write (does not fit in int)");
    return;
  }
  char prefix[16];
  int prefixlen = snprintf(prefix, sizeof(prefix), "%c %d\n", type, len);
  if (prefixlen <= 0 || (size_t)prefixlen >= sizeof(prefix)) {
    Log("overlong prefix, cannot write");
    return;
  }
  // Yes, we're wasting syscalls here. This doesn't need to be fast though, and
  // this way we can avoid an extra buffer.
  if (!WriteChars(fd, prefix, prefixlen)) {
    return;
  }
  if (len != 0 && !WriteChars(fd, message, len)) {
    return;
  }
  if (!WriteChars(fd, "\n", 1)) {
    return;
  }
}

static size_t ReadChars(int fd, char *buf, size_t n, int eof_permitted) {
  size_t total = 0;
  while (total < n) {
    ssize_t got = RetryRead(fd, buf + total, n - total);
    if (got < 0) {
      LogErrno("read");
      return 0;
    }
    if (got == 0) {
      if (!eof_permitted) {
        Log("read: unexpected end of file");
        return 0;
      }
      break;
    }
    if ((size_t)got > n - total) {
      Log("read: overlong read (should never happen)");
    }
    total += got;
  }
  return total;
}

static void ClearAndFreePacket(char **message, size_t len) {
  if (*message == NULL) {
    return;
  }
  explicit_bzero(*message, len + 1);
  free(*message);
  *message = NULL;
}

char ReadPacket(int fd, char **message, int eof_permitted) {
  char type;
  *message = NULL;
  if (!ReadChars(fd, &type, 1, eof_permitted)) {
    return 0;
  }
  if (type == 0) {
    Log("invalid packet type 0");
    return 0;
  }
  char c;
  if (!ReadChars(fd, &c, 1, 0)) {
    return 0;
  }
  if (c != ' ') {
    Log("invalid character after packet type, expecting space");
    return 0;
  }
  int len = 0;
  for (int digits = 0; digits <= MAX_PACKET_LENGTH_DIGITS; ++digits) {
    if (!ReadChars(fd, &c, 1, 0)) {
      return 0;
    }
    if (c == '\n') {
      if (digits == 0) {
        Log("missing packet length");
        return 0;
      }
      goto have_len;
    }
    if (c < '0' || c > '9') {
      Log("invalid character during packet length, expecting 0-9 or newline");
      return 0;
    }
    if (digits == MAX_PACKET_LENGTH_DIGITS) {
      Log("overlong packet length");
      return 0;
    }
    int digit = c - '0';
    if (len > (MAX_PACKET_LENGTH - digit) / 10) {
      Log("invalid packet length");
      return 0;
    }
    len = len * 10 + digit;
  }
have_len:
  if (len < 0 || len > MAX_PACKET_LENGTH) {
    Log("invalid length %d", len);
    return 0;
  }
  *message = malloc((size_t)len + 1);
  if (*message == NULL) {
    LogErrno("malloc");
    return 0;
  }
  if ((type == PTYPE_RESPONSE_LIKE_PASSWORD) &&
      MLOCK_PAGE(*message, len + 1) < 0) {
    // We continue anyway, as the user being unable to unlock the screen is
    // worse.
    LogErrno("mlock");
  }
  if (len != 0 && !ReadChars(fd, *message, len, 0)) {
    ClearAndFreePacket(message, (size_t)len);
    return 0;
  }
  (*message)[len] = 0;
  if (!ReadChars(fd, &c, 1, 0)) {
    ClearAndFreePacket(message, (size_t)len);
    return 0;
  }
  if (c != '\n') {
    Log("invalid character after packet message, expecting newline");
    ClearAndFreePacket(message, (size_t)len);
    return 0;
  }
  return type;
}
