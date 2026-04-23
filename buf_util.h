// SPDX-License-Identifier: Apache-2.0

#ifndef XSECURELOCK_BUF_UTIL_H
#define XSECURELOCK_BUF_UTIL_H

#include <stddef.h>  // for size_t

int AppendBytes(char **dst, size_t *remaining, const char *src, size_t len);
int AppendCString(char **dst, size_t *remaining, const char *src);
void ClearFreeString(char **p);
void ClearFreeBuffer(char **p, size_t len);

#endif  // XSECURELOCK_BUF_UTIL_H
