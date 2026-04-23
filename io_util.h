// io helper header file
//
// Copyright 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef XSECURELOCK_IO_UTIL_H
#define XSECURELOCK_IO_UTIL_H

#include <poll.h>       // for pollfd, nfds_t
#include <stddef.h>     // for size_t
#include <sys/types.h>  // for ssize_t

/*! \brief Closes a file descriptor if it is nonnegative.
 *
 * On success, *fd is set to -1.
 *
 * \param fd Pointer to the descriptor number to close.
 * \return Zero on success, or -1 on failure.
 */
int CloseIfValid(int *fd);

/*! \brief Closes both entries in a descriptor pair.
 *
 * Both entries are passed through CloseIfValid(), so successfully closed
 * descriptors are reset to -1.
 *
 * \param fds The descriptor pair to close.
 * \return Zero if both closes succeeded, or -1 if either close failed.
 */
int ClosePair(int fds[2]);

/*! \brief Moves a file descriptor onto a target descriptor number.
 *
 * If *fd already equals target_fd, this is a no-op. Otherwise, the old *fd is
 * dup2()'d onto target_fd, the original descriptor is closed, and *fd is
 * updated to target_fd.
 *
 * \param fd Pointer to the source descriptor number.
 * \param target_fd The destination descriptor number.
 * \return Zero on success, or -1 on failure.
 */
int MoveFdTo(int *fd, int target_fd);

/*! \brief Enables close-on-exec on a file descriptor.
 *
 * \param fd The descriptor to update.
 * \return Zero on success, or -1 on failure.
 */
int SetFdCloexec(int fd);

/*! \brief Enables nonblocking mode on a file descriptor.
 *
 * \param fd The descriptor to update.
 * \return Zero on success, or -1 on failure.
 */
int SetFdNonblocking(int fd);

/*! \brief Creates a pipe whose descriptors have close-on-exec enabled.
 *
 * \param fds Output array receiving the read end in fds[0] and the write end in
 *   fds[1].
 * \return Zero on success, or -1 on failure.
 */
int PipeCloexec(int fds[2]);

/*! \brief Reads from a descriptor, retrying after EINTR.
 *
 * \param fd The descriptor to read from.
 * \param buf Output buffer.
 * \param len Maximum number of bytes to read.
 * \return The read() result, except EINTR is retried internally.
 */
ssize_t RetryRead(int fd, void *buf, size_t len);

/*! \brief Writes to a descriptor, retrying after EINTR.
 *
 * \param fd The descriptor to write to.
 * \param buf Input buffer.
 * \param len Number of bytes to write.
 * \return The write() result, except EINTR is retried internally.
 */
ssize_t RetryWrite(int fd, const void *buf, size_t len);

/*! \brief Reads exactly len bytes unless EOF or an error occurs first.
 *
 * \param fd The descriptor to read from.
 * \param buf Output buffer.
 * \param len Number of bytes to read.
 * \return The total number of bytes read, which may be less than len on EOF, or
 *   -1 on error.
 */
ssize_t ReadFull(int fd, void *buf, size_t len);

/*! \brief Writes exactly len bytes unless an error occurs first.
 *
 * \param fd The descriptor to write to.
 * \param buf Input buffer.
 * \param len Number of bytes to write.
 * \return len on success, or -1 on error.
 */
ssize_t WriteFull(int fd, const void *buf, size_t len);

/*! \brief Polls descriptors, retrying after EINTR.
 *
 * \param fds Poll descriptor array.
 * \param nfds Number of entries in fds.
 * \param timeout_ms Timeout in milliseconds, as for poll().
 * \return The poll() result, except EINTR is retried internally.
 */
int RetryPoll(struct pollfd *fds, nfds_t nfds, int timeout_ms);

#endif  // XSECURELOCK_IO_UTIL_H
