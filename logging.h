/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef LOGGING_H
#define LOGGING_H

#include "util.h"

/*! \brief Prints the given string to the error log (stderr).
 *
 * For a format expanding to "Foo", this will log "xsecurelock: Foo.".
 *
 * \param format A printf format string, followed by its arguments.
 */
void Log(const char *format, ...) XSL_PRINTF(1, 2);

/*! \brief Prints the given string to the error log (stderr).
 *
 * For a format expanding to "Foo", this may log "xsecurelock: Foo: No such
 * file or directory". The value of errno is preserved by this function.
 *
 * \param format A printf format string, followed by its arguments.
 */
void LogErrno(const char *format, ...) XSL_PRINTF(1, 2);

#endif
