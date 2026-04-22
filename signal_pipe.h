#ifndef XSECURELOCK_SIGNAL_PIPE_H
#define XSECURELOCK_SIGNAL_PIPE_H

struct SignalPipe {
  int fds[2];
};

int SignalPipeInit(struct SignalPipe *pipe);
void SignalPipeSetWriteFdForHandler(int fd);
void SignalPipeNotifyFromHandler(void);
void SignalPipeDrain(int fd, const char *label);
void SignalPipeClose(struct SignalPipe *pipe);

#endif  // XSECURELOCK_SIGNAL_PIPE_H
