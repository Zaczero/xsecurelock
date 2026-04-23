// SPDX-License-Identifier: Apache-2.0

#ifndef XSECURELOCK_TIME_UTIL_H
#define XSECURELOCK_TIME_UTIL_H

#include <stdint.h>  // for int64_t

int GetMonotonicTimeMs(int64_t *time_ms);
int SleepMs(int timeout_ms);
int SleepNs(int64_t timeout_ns);

#endif  // XSECURELOCK_TIME_UTIL_H
