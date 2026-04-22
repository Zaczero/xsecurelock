#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "../helpers/authproto.h"
#include "../time_util.h"
#include "../util.h"

static volatile sig_atomic_t signal_count = 0;

static void HandleSignal(int signo) {
  (void)signo;
  ++signal_count;
}

static void InstallSignalHandler(void) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = HandleSignal;
  if (sigaction(SIGALRM, &sa, NULL) != 0) {
    perror("sigaction");
    exit(1);
  }
}

static void StartTimerMs(int milliseconds) {
  struct itimerval timer;
  memset(&timer, 0, sizeof(timer));
  timer.it_value.tv_sec = milliseconds / 1000;
  timer.it_value.tv_usec = (milliseconds % 1000) * 1000;
  if (setitimer(ITIMER_REAL, &timer, NULL) != 0) {
    perror("setitimer");
    exit(1);
  }
}

static void StopTimer(void) {
  struct itimerval timer;
  memset(&timer, 0, sizeof(timer));
  if (setitimer(ITIMER_REAL, &timer, NULL) != 0) {
    perror("setitimer");
    exit(1);
  }
}

static void WaitForChildOrDie(pid_t childpid) {
  int status;
  while (waitpid(childpid, &status, 0) < 0) {
    if (errno != EINTR) {
      perror("waitpid");
      exit(1);
    }
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    fprintf(stderr, "child exited unexpectedly\n");
    exit(1);
  }
}

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

static void ExpectInterruptedReadSuccess(void) {
  int fds[2];
  if (pipe(fds) != 0) {
    perror("pipe");
    exit(1);
  }

  pid_t childpid = fork();
  if (childpid < 0) {
    perror("fork");
    exit(1);
  }
  if (childpid == 0) {
    close(fds[0]);
    if (SleepMs(200) != 0) {
      perror("SleepMs");
      _exit(1);
    }
    WriteAllOrDie(fds[1], "P 7\nhunter2\n", strlen("P 7\nhunter2\n"));
    close(fds[1]);
    _exit(0);
  }

  close(fds[1]);
  signal_count = 0;
  StartTimerMs(50);

  char *message = NULL;
  char type = ReadPacket(fds[0], &message, 0);

  StopTimer();
  close(fds[0]);
  WaitForChildOrDie(childpid);

  if (type != 'P') {
    fprintf(stderr, "interrupted-read: expected type P, got %d\n", (int)type);
    free(message);
    exit(1);
  }
  if (message == NULL || strcmp(message, "hunter2") != 0) {
    fprintf(stderr, "interrupted-read: unexpected message\n");
    free(message);
    exit(1);
  }
  if (signal_count == 0) {
    fprintf(stderr, "interrupted-read: expected signal delivery\n");
    free(message);
    exit(1);
  }

  explicit_bzero(message, strlen(message));
  free(message);
}

int main(void) {
  InstallSignalHandler();
  ExpectReadFailure("missing-length", "P \n", 3);
  ExpectReadFailure("too-many-digits", "P 123456\nx\n", 11);
  ExpectReadFailure("length-above-cap", "P 65535\nx\n", 10);
  ExpectReadFailure("bad-trailing-newline", "P 1\naX", 6);
  ExpectInterruptedReadSuccess();
  return 0;
}
