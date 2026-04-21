#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

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
  assert(sigaction(SIGALRM, &sa, NULL) == 0);
}

static void StartTimerMs(int milliseconds) {
  struct itimerval timer;
  memset(&timer, 0, sizeof(timer));
  timer.it_value.tv_sec = milliseconds / 1000;
  timer.it_value.tv_usec = (milliseconds % 1000) * 1000;
  assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
}

static void StopTimer(void) {
  struct itimerval timer;
  memset(&timer, 0, sizeof(timer));
  assert(setitimer(ITIMER_REAL, &timer, NULL) == 0);
}

static void SleepMs(int milliseconds) {
  struct timespec delay;
  delay.tv_sec = milliseconds / 1000;
  delay.tv_nsec = (milliseconds % 1000) * 1000000L;
  while (nanosleep(&delay, &delay) != 0) {
    assert(errno == EINTR);
  }
}

static void WaitForChild(pid_t childpid) {
  int status;
  while (waitpid(childpid, &status, 0) < 0) {
    assert(errno == EINTR);
  }
  assert(WIFEXITED(status));
  assert(WEXITSTATUS(status) == 0);
}

static void FillPipe(int fd) {
  int flags = fcntl(fd, F_GETFL);
  assert(flags >= 0);
  assert(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0);

  char buf[4096];
  memset(buf, 'x', sizeof(buf));
  for (;;) {
    ssize_t written = write(fd, buf, sizeof(buf));
    if (written > 0) {
      continue;
    }
    assert(written < 0);
    assert(errno == EAGAIN || errno == EWOULDBLOCK);
    break;
  }

  assert(fcntl(fd, F_SETFL, flags) == 0);
}

static void TestRetryRead(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  pid_t childpid = fork();
  assert(childpid >= 0);
  if (childpid == 0) {
    close(fds[0]);
    SleepMs(200);
    assert(write(fds[1], "A", 1) == 1);
    close(fds[1]);
    _exit(0);
  }

  close(fds[1]);
  signal_count = 0;
  StartTimerMs(50);

  char ch = 0;
  ssize_t got = RetryRead(fds[0], &ch, 1);

  StopTimer();
  close(fds[0]);
  WaitForChild(childpid);

  assert(got == 1);
  assert(ch == 'A');
  assert(signal_count > 0);
}

static void TestRetryWrite(void) {
  int fds[2];
  assert(pipe(fds) == 0);
  FillPipe(fds[1]);

  pid_t childpid = fork();
  assert(childpid >= 0);
  if (childpid == 0) {
    close(fds[1]);
    SleepMs(200);
    char drain[4096];
    assert(read(fds[0], drain, sizeof(drain)) > 0);
    close(fds[0]);
    _exit(0);
  }

  signal_count = 0;
  StartTimerMs(50);

  char ch = 'B';
  ssize_t written = RetryWrite(fds[1], &ch, 1);

  StopTimer();
  close(fds[1]);
  close(fds[0]);
  WaitForChild(childpid);

  assert(written == 1);
  assert(signal_count > 0);
}

static void TestRetryPoll(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  pid_t childpid = fork();
  assert(childpid >= 0);
  if (childpid == 0) {
    close(fds[0]);
    SleepMs(200);
    assert(write(fds[1], "C", 1) == 1);
    close(fds[1]);
    _exit(0);
  }

  close(fds[1]);
  signal_count = 0;
  StartTimerMs(50);

  struct pollfd pfd;
  pfd.fd = fds[0];
  pfd.events = POLLIN | POLLHUP;
  pfd.revents = 0;

  int ready = RetryPoll(&pfd, 1, 1000);

  StopTimer();

  assert(ready == 1);
  assert(pfd.revents & (POLLIN | POLLHUP));
  assert(signal_count > 0);

  char ch;
  assert(read(fds[0], &ch, 1) == 1);
  assert(ch == 'C');
  close(fds[0]);
  WaitForChild(childpid);
}

static void TestRetryPollHighFd(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  int high_fd = fcntl(fds[0], F_DUPFD, FD_SETSIZE + 16);
  if (high_fd < 0) {
    if (errno == EINVAL || errno == EMFILE) {
      fprintf(stderr, "retry_io_test: skipping high-fd poll case\n");
      close(fds[0]);
      close(fds[1]);
      return;
    }
    assert(0);
  }
  close(fds[0]);

  pid_t childpid = fork();
  assert(childpid >= 0);
  if (childpid == 0) {
    close(high_fd);
    SleepMs(200);
    assert(write(fds[1], "D", 1) == 1);
    close(fds[1]);
    _exit(0);
  }

  close(fds[1]);
  signal_count = 0;
  StartTimerMs(50);

  struct pollfd pfd;
  pfd.fd = high_fd;
  pfd.events = POLLIN | POLLHUP;
  pfd.revents = 0;

  int ready = RetryPoll(&pfd, 1, 1000);

  StopTimer();

  assert(ready == 1);
  assert(pfd.revents & (POLLIN | POLLHUP));
  assert(signal_count > 0);

  char ch;
  assert(read(high_fd, &ch, 1) == 1);
  assert(ch == 'D');
  close(high_fd);
  WaitForChild(childpid);
}

int main(void) {
  InstallSignalHandler();
  TestRetryRead();
  TestRetryWrite();
  TestRetryPoll();
  TestRetryPollHighFd();
  return 0;
}
