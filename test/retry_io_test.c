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
#include <unistd.h>

#include "../io_util.h"
#include "../time_util.h"

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
    assert(SleepMs(200) == 0);
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
    assert(SleepMs(200) == 0);
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
    assert(SleepMs(200) == 0);
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

static void TestReadFull(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  pid_t childpid = fork();
  assert(childpid >= 0);
  if (childpid == 0) {
    close(fds[0]);
    assert(SleepMs(200) == 0);
    assert(write(fds[1], "FG", 2) == 2);
    close(fds[1]);
    _exit(0);
  }

  close(fds[1]);
  signal_count = 0;
  StartTimerMs(50);

  char buf[2] = {0};
  ssize_t got = ReadFull(fds[0], buf, sizeof(buf));

  StopTimer();
  close(fds[0]);
  WaitForChild(childpid);

  assert(got == 2);
  assert(buf[0] == 'F');
  assert(buf[1] == 'G');
  assert(signal_count > 0);
}

static void TestReadFullEof(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  assert(write(fds[1], "H", 1) == 1);
  close(fds[1]);

  char buf[2] = {0};
  ssize_t got = ReadFull(fds[0], buf, sizeof(buf));
  close(fds[0]);

  assert(got == 1);
  assert(buf[0] == 'H');
}

static void TestWriteFull(void) {
  int fds[2];
  assert(pipe(fds) == 0);
  FillPipe(fds[1]);

  pid_t childpid = fork();
  assert(childpid >= 0);
  if (childpid == 0) {
    close(fds[1]);
    assert(SleepMs(200) == 0);
    char drain[4096];
    ssize_t got = read(fds[0], drain, sizeof(drain));
    assert(got >= 2);
    close(fds[0]);
    _exit(0);
  }

  signal_count = 0;
  StartTimerMs(50);

  ssize_t written = WriteFull(fds[1], "IJ", 2);

  StopTimer();
  close(fds[1]);
  close(fds[0]);
  WaitForChild(childpid);

  assert(written == 2);
  assert(signal_count > 0);
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
    assert(SleepMs(200) == 0);
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

static void TestPipeCloexec(void) {
  int fds[2];
  assert(PipeCloexec(fds) == 0);

  int flags0 = fcntl(fds[0], F_GETFD);
  int flags1 = fcntl(fds[1], F_GETFD);
  assert(flags0 >= 0);
  assert(flags1 >= 0);
  assert(flags0 & FD_CLOEXEC);
  assert(flags1 & FD_CLOEXEC);

  assert(write(fds[1], "E", 1) == 1);
  char ch = 0;
  assert(read(fds[0], &ch, 1) == 1);
  assert(ch == 'E');

  close(fds[0]);
  close(fds[1]);
}

static void TestCloseIfValid(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  int closed_fd = fds[0];
  assert(CloseIfValid(&fds[0]) == 0);
  assert(fds[0] == -1);
  assert(fcntl(closed_fd, F_GETFD) < 0);
  assert(errno == EBADF);

  assert(CloseIfValid(&fds[0]) == 0);
  close(fds[1]);
}

static void TestClosePair(void) {
  int fds[2];
  assert(pipe(fds) == 0);

  int fd0 = fds[0];
  int fd1 = fds[1];
  assert(ClosePair(fds) == 0);
  assert(fds[0] == -1);
  assert(fds[1] == -1);
  assert(fcntl(fd0, F_GETFD) < 0);
  assert(errno == EBADF);
  assert(fcntl(fd1, F_GETFD) < 0);
  assert(errno == EBADF);
}

static void TestGetMonotonicTimeMs(void) {
  int64_t start_ms = 0;
  int64_t end_ms = 0;

  assert(GetMonotonicTimeMs(&start_ms) == 0);
  assert(SleepMs(20) == 0);
  assert(GetMonotonicTimeMs(&end_ms) == 0);
  assert(end_ms >= start_ms);
  assert(end_ms - start_ms >= 1);
}

static void TestSleepMs(void) {
  int64_t start_ms = 0;
  int64_t end_ms = 0;

  errno = 0;
  assert(SleepMs(-1) < 0);
  assert(errno == EINVAL);

  signal_count = 0;
  StartTimerMs(20);
  assert(GetMonotonicTimeMs(&start_ms) == 0);
  assert(SleepMs(80) == 0);
  assert(GetMonotonicTimeMs(&end_ms) == 0);
  StopTimer();

  assert(end_ms >= start_ms);
  assert(end_ms - start_ms >= 20);
  assert(signal_count > 0);
}

int main(void) {
  InstallSignalHandler();
  TestRetryRead();
  TestRetryWrite();
  TestRetryPoll();
  TestReadFull();
  TestReadFullEof();
  TestWriteFull();
  TestRetryPollHighFd();
  TestPipeCloexec();
  TestCloseIfValid();
  TestClosePair();
  TestGetMonotonicTimeMs();
  TestSleepMs();
  return 0;
}
