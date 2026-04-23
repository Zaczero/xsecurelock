// SPDX-License-Identifier: Apache-2.0

#ifndef XSECURELOCK_CONFIGURED_COMMAND_H
#define XSECURELOCK_CONFIGURED_COMMAND_H

#include <stddef.h>  // for size_t

int FormatKeyCommandEnvName(char *dst, size_t dst_size, const char *keyname);
int RunShellCommandValue(const char *label, const char *command, int background);
int RunShellCommandFromEnv(const char *env_name, int background);

#endif  // XSECURELOCK_CONFIGURED_COMMAND_H
