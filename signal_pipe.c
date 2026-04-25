#include "config.h"

#include "signal_pipe.h"

#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include "io_util.h"
#include "logging.h"

static volatile sig_atomic_t g_signal_pipe_write_fd = -1;

int SignalPipeInit(struct SignalPipe *pipe) {
  if (pipe == NULL) {
    errno = EINVAL;
    return -1;
  }

  pipe->fds[0] = -1;
  pipe->fds[1] = -1;
  if (PipeCloexec(pipe->fds) != 0) {
    return -1;
  }
  if (SetFdNonblocking(pipe->fds[0]) != 0 ||
      SetFdNonblocking(pipe->fds[1]) != 0) {
    int saved_errno = errno;
    SignalPipeClose(pipe);
    errno = saved_errno;
    return -1;
  }
  return 0;
}

int SignalPipeSetWriteFdForHandler(int fd) {
  if (fd < 0) {
    g_signal_pipe_write_fd = -1;
    return 1;
  }

  sig_atomic_t signal_fd = (sig_atomic_t)fd;
  if ((int)signal_fd != fd) {
    errno = EOVERFLOW;
    return 0;
  }

  g_signal_pipe_write_fd = signal_fd;
  return 1;
}

void SignalPipeNotifyFromHandler(void) {
  sig_atomic_t write_fd = g_signal_pipe_write_fd;
  if (write_fd < 0) {
    return;
  }

  char signal_byte = 'x';
  ssize_t unused = write((int)write_fd, &signal_byte, 1);
  (void)unused;
}

void SignalPipeDrain(int fd, const char *label) {
  const char *name = (label != NULL && *label != '\0') ? label : "signal pipe";

  for (;;) {
    char buf[32];
    ssize_t got = read(fd, buf, sizeof(buf));
    if (got > 0) {
      continue;
    }
    if (got == 0) {
      return;
    }
    if (errno == EINTR) {
      continue;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    LogErrno("read(%s)", name);
    return;
  }
}

void SignalPipeClose(struct SignalPipe *pipe) {
  if (pipe == NULL) {
    return;
  }
  if (g_signal_pipe_write_fd == pipe->fds[1]) {
    (void)SignalPipeSetWriteFdForHandler(-1);
  }
  (void)ClosePair(pipe->fds);
}
