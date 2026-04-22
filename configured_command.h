/*
Copyright 2018 Google Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

#ifndef XSECURELOCK_CONFIGURED_COMMAND_H
#define XSECURELOCK_CONFIGURED_COMMAND_H

#include <stddef.h>  // for size_t

int FormatKeyCommandEnvName(char *dst, size_t dst_size, const char *keyname);
int RunShellCommandValue(const char *label, const char *command, int background);
int RunShellCommandFromEnv(const char *env_name, int background);

#endif  // XSECURELOCK_CONFIGURED_COMMAND_H
