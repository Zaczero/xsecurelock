#ifndef XSECURELOCK_SIGNAL_PIPE_H
#define XSECURELOCK_SIGNAL_PIPE_H

/*! \brief A self-pipe used to wake a poll/select loop from a signal handler.
 *
 * The read end is stored in fds[0] and the write end in fds[1]. Callers own the
 * lifetime of the pipe and should close it with SignalPipeClose().
 */
struct SignalPipe {
  int fds[2];
};

/*! \brief Initializes a signal pipe.
 *
 * On success, both file descriptors are created with close-on-exec and
 * nonblocking mode enabled.
 *
 * \param pipe The pipe object to initialize.
 * \return Zero if initialization succeeded, or -1 on failure.
 */
int SignalPipeInit(struct SignalPipe *pipe);

/*! \brief Selects which file descriptor SignalPipeNotifyFromHandler() writes
 * to.
 *
 * The caller is responsible for passing the current write end, or -1 to disable
 * notifications.
 *
 * \return 1 if the file descriptor was accepted, or 0 if it cannot be
 *   represented safely in signal-handler shared state.
 */
int SignalPipeSetWriteFdForHandler(int fd);

/*! \brief Wakes the main loop from a signal handler.
 *
 * This helper is async-signal-safe. It writes a single byte to the file
 * descriptor previously selected by SignalPipeSetWriteFdForHandler(), and
 * silently does nothing if notifications are disabled.
 */
void SignalPipeNotifyFromHandler(void);

/*! \brief Drains all pending wakeup bytes from the read end of a signal pipe.
 *
 * \param fd The read end to drain.
 * \param label Optional log label used if draining fails.
 */
void SignalPipeDrain(int fd, const char *label);

/*! \brief Closes a signal pipe and disables handler writes to it.
 *
 * Passing NULL is allowed.
 */
void SignalPipeClose(struct SignalPipe *pipe);

#endif  // XSECURELOCK_SIGNAL_PIPE_H
