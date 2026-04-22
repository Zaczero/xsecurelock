#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "../signal_pipe.h"

static void TestSignalPipeInit(void) {
  struct SignalPipe pipe;
  assert(SignalPipeInit(&pipe) == 0);
  assert(pipe.fds[0] >= 0);
  assert(pipe.fds[1] >= 0);

  int read_fd_flags = fcntl(pipe.fds[0], F_GETFD);
  int write_fd_flags = fcntl(pipe.fds[1], F_GETFD);
  int read_status_flags = fcntl(pipe.fds[0], F_GETFL);
  int write_status_flags = fcntl(pipe.fds[1], F_GETFL);
  assert(read_fd_flags >= 0);
  assert(write_fd_flags >= 0);
  assert(read_status_flags >= 0);
  assert(write_status_flags >= 0);
  assert(read_fd_flags & FD_CLOEXEC);
  assert(write_fd_flags & FD_CLOEXEC);
  assert(read_status_flags & O_NONBLOCK);
  assert(write_status_flags & O_NONBLOCK);

  SignalPipeClose(&pipe);
  assert(pipe.fds[0] == -1);
  assert(pipe.fds[1] == -1);
}

static void TestSignalPipeNotify(void) {
  struct SignalPipe pipe;
  assert(SignalPipeInit(&pipe) == 0);
  SignalPipeSetWriteFdForHandler(pipe.fds[1]);

  SignalPipeNotifyFromHandler();

  char ch = 0;
  assert(read(pipe.fds[0], &ch, 1) == 1);
  assert(ch == 'x');

  SignalPipeClose(&pipe);
}

static void TestSignalPipeDrain(void) {
  struct SignalPipe pipe;
  assert(SignalPipeInit(&pipe) == 0);
  SignalPipeSetWriteFdForHandler(pipe.fds[1]);

  SignalPipeNotifyFromHandler();
  SignalPipeNotifyFromHandler();
  SignalPipeNotifyFromHandler();
  SignalPipeDrain(pipe.fds[0], "signal pipe");

  char ch = 0;
  assert(read(pipe.fds[0], &ch, 1) < 0);
  assert(errno == EAGAIN || errno == EWOULDBLOCK);

  SignalPipeClose(&pipe);
}

int main(void) {
  TestSignalPipeInit();
  TestSignalPipeNotify();
  TestSignalPipeDrain();
  return 0;
}
