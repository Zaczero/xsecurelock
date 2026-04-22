// util header file
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

#ifndef XSECURELOCK_UTIL_H
#define XSECURELOCK_UTIL_H

#include <stddef.h>  // for size_t

#if defined(__GNUC__) || defined(__clang__)
#define XSL_PRINTF(string_index, first_to_check) \
  __attribute__((format(printf, string_index, first_to_check)))
#else
#define XSL_PRINTF(string_index, first_to_check)
#endif

#define XSL_CONCAT_IMPL(a, b) a##b
#define XSL_CONCAT(a, b) XSL_CONCAT_IMPL(a, b)

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define XSL_STATIC_ASSERT(expr, message) _Static_assert((expr), message)
#else
#define XSL_STATIC_ASSERT(expr, message)                              \
  typedef char XSL_CONCAT(xsl_static_assert_, __LINE__)[(expr) ? 1 : \
                                                             -1]
#endif

#define ARRAY_LEN(array) (sizeof(array) / sizeof((array)[0]))

// Use the libc implementation when available; otherwise util.c provides a
// local fallback with the same signature.
void explicit_bzero(void *s, size_t len);

#endif  // XSECURELOCK_UTIL_H
