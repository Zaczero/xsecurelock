#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../helpers/authproto.h"

static void WriteAllOrDie(int fd, const char *buf, size_t len) {
  size_t total = 0;
  while (total < len) {
    ssize_t got = write(fd, buf + total, len - total);
    if (got <= 0) {
      perror("write");
      exit(1);
    }
    total += (size_t)got;
  }
}

static void ExpectReadFailure(const char *name, const char *input,
                              size_t input_len) {
  int fds[2];
  if (pipe(fds) != 0) {
    perror("pipe");
    exit(1);
  }
  WriteAllOrDie(fds[1], input, input_len);
  close(fds[1]);

  char *message = (char *)1;
  char type = ReadPacket(fds[0], &message, 0);
  close(fds[0]);

  if (type != 0) {
    fprintf(stderr, "%s: expected failure, got type %d\n", name, (int)type);
    free(message);
    exit(1);
  }
  if (message != NULL) {
    fprintf(stderr, "%s: expected NULL message on failure\n", name);
    free(message);
    exit(1);
  }
}

int main(void) {
  ExpectReadFailure("missing-length", "P \n", 3);
  ExpectReadFailure("too-many-digits", "P 123456\nx\n", 11);
  ExpectReadFailure("length-above-cap", "P 65535\nx\n", 10);
  ExpectReadFailure("bad-trailing-newline", "P 1\naX", 6);
  return 0;
}
