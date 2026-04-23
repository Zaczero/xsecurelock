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

#include "config.h"

#include "authproto.h"

#include <stdio.h>   // for snprintf
#include <stdlib.h>  // for malloc, size_t
#include <string.h>  // for strlen
#include <unistd.h>  // for read, write, ssize_t

#include "../buf_util.h"    // for ClearFreeBuffer
#include "../io_util.h"     // for ReadFull, WriteFull
#include "../logging.h"     // for LogErrno, Log
#include "../mlock_page.h"  // for MLOCK_PAGE

#define MAX_PACKET_LENGTH 0xFFFE
#define MAX_PACKET_LENGTH_DIGITS 5

static int WriteExact(int fd, const void *buf, size_t len) {
  if (WriteFull(fd, buf, len) == (ssize_t)len) {
    return 1;
  }
  LogErrno("write");
  return 0;
}

int WritePacketBytes(int fd, char type, const char *message, size_t len) {
  if (len > MAX_PACKET_LENGTH) {
    Log("overlong message, cannot write (hardcoded limit)");
    return 0;
  }
  char prefix[16];
  int prefixlen = snprintf(prefix, sizeof(prefix), "%c %zu\n", type, len);
  if (prefixlen <= 0 || (size_t)prefixlen >= sizeof(prefix)) {
    Log("overlong prefix, cannot write");
    return 0;
  }
  // Yes, we're wasting syscalls here. This doesn't need to be fast though, and
  // this way we can avoid an extra buffer.
  if (!WriteExact(fd, prefix, (size_t)prefixlen)) {
    return 0;
  }
  if (len != 0 && !WriteExact(fd, message, len)) {
    return 0;
  }
  if (!WriteExact(fd, "\n", 1)) {
    return 0;
  }
  return 1;
}

void WritePacket(int fd, char type, const char *message) {
  (void)WritePacketBytes(fd, type, message, strlen(message));
}

static int ReadExact(int fd, void *buf, size_t len, int eof_permitted) {
  ssize_t got = ReadFull(fd, buf, len);
  if (got == (ssize_t)len) {
    return 1;
  }
  if (got < 0) {
    LogErrno("read");
    return 0;
  }
  if (eof_permitted && got == 0) {
    return 0;
  }
  Log("read: unexpected end of file");
  return -1;
}

static void ClearAndFreePacket(char **message, size_t len) {
  ClearFreeBuffer(message, len + 1);
}

char ReadPacket(int fd, char **message, int eof_permitted) {
  char type = 0;
  *message = NULL;
  if (ReadExact(fd, &type, 1, eof_permitted) <= 0) {
    return 0;
  }
  if (type == 0) {
    Log("invalid packet type 0");
    return 0;
  }
  char c;
  if (ReadExact(fd, &c, 1, 0) <= 0) {
    return 0;
  }
  if (c != ' ') {
    Log("invalid character after packet type, expecting space");
    return 0;
  }
  int len = 0;
  for (int digits = 0; digits <= MAX_PACKET_LENGTH_DIGITS; ++digits) {
    if (ReadExact(fd, &c, 1, 0) <= 0) {
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
  if (len != 0 && ReadExact(fd, *message, (size_t)len, 0) <= 0) {
    ClearAndFreePacket(message, (size_t)len);
    return 0;
  }
  (*message)[len] = 0;
  if (ReadExact(fd, &c, 1, 0) <= 0) {
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
