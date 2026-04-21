/*
 * Copyright 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *
 * An earlier version of this file was originally released into the public
 * domain by its authors. It has been modified to make the code compile and
 * link as part of the Google Authenticator project. These changes are
 * copyrighted by Google Inc. and released under the Apache License,
 * Version 2.0.
 *
 * The previous authors' terms are included below:
 */

/*****************************************************************************
 *
 * File:    util.c
 *
 * Purpose: Collection of cross file utility functions.
 *
 * This code is in the public domain
 *
 *****************************************************************************
 */

#include "config.h"

#include <string.h>

#ifndef HAVE_EXPLICIT_BZERO
#define HAVE_EXPLICIT_BZERO 0
#endif

#ifndef FORCE_EXPLICIT_BZERO_FALLBACK
#define FORCE_EXPLICIT_BZERO_FALLBACK 0
#endif

#if !HAVE_EXPLICIT_BZERO || FORCE_EXPLICIT_BZERO_FALLBACK
// Prefer libc explicit_bzero() when it exists. Otherwise, route memset()
// through a volatile function pointer so the wipe remains an observable call
// without relying on compiler-specific inline assembly.
static void *(*const volatile memset_impl)(void *, int, size_t) = memset;

void explicit_bzero(void *s, size_t len) {
  memset_impl(s, 0, len);
}
#endif
