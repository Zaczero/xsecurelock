// SPDX-License-Identifier: Apache-2.0

#ifndef XSECURELOCK_HELPERS_AUTH_TITLE_H
#define XSECURELOCK_HELPERS_AUTH_TITLE_H

#include <stdbool.h>
#include <stddef.h>

void AuthBuildTitle(char *output, size_t output_size, const char *auth_title,
                    bool show_username, int show_hostname, const char *username,
                    const char *hostname, const char *input);

#endif  // XSECURELOCK_HELPERS_AUTH_TITLE_H
