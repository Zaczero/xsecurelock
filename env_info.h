/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ENV_INFO_H
#define ENV_INFO_H

#include <stddef.h>

/*! \brief Loads the current host name.
 *
 * \param hostname_buf The buffer to write the host name to.
 * \param hostname_buflen The size of the buffer.
 * \return Whether fetching the host name succeeded.
 */
int GetHostName(char* hostname_buf, size_t hostname_buflen);

/*! \brief Loads the current user name.
 *
 * \param username_buf The buffer to write the user name to.
 * \param username_buflen The size of the buffer.
 * \return Whether fetching the user name succeeded.
 */
int GetUserName(char* username_buf, size_t username_buflen);

#endif
