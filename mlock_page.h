/*
 * Copyright 2014 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

// This is a helper to support mlock on non page aligned data. It will simply
// lock the whole page covering the data.

#ifndef MLOCK_PAGE_H
#define MLOCK_PAGE_H

#include "config.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef HAVE_MLOCK
#define HAVE_MLOCK 0
#endif

#ifndef HAVE_MLOCKALL
#define HAVE_MLOCKALL 0
#endif

#ifndef FORCE_MLOCK_PAGE_UNAVAILABLE
#define FORCE_MLOCK_PAGE_UNAVAILABLE 0
#endif

/*! \brief Lock the memory area given by a pointer and a size.
 *
 * The area is expanded to fill whole memory pages when mlock() is available.
 */
static inline int MlockPage(const void *ptr, size_t size) {
  if (size == 0) {
    return 0;
  }

#if FORCE_MLOCK_PAGE_UNAVAILABLE || !HAVE_MLOCK
  (void)ptr;
#endif

#if FORCE_MLOCK_PAGE_UNAVAILABLE
  errno = ENOSYS;
  return -1;
#elif HAVE_MLOCK
  long page_size = sysconf(_SC_PAGESIZE);
  if (page_size <= 0) {
#if HAVE_MLOCKALL
    return mlockall(MCL_CURRENT);
#else
    errno = ENOSYS;
    return -1;
#endif
  }

  uintptr_t page_size_u = (uintptr_t)page_size;
  uintptr_t ptr_u = (uintptr_t)ptr;
  if (ptr_u > UINTPTR_MAX - (size - 1)) {
    errno = EINVAL;
    return -1;
  }
  uintptr_t last = ptr_u + size - 1;
  uintptr_t start = ptr_u - (ptr_u % page_size_u);
  uintptr_t last_page = last - (last % page_size_u);
  if (last_page > UINTPTR_MAX - page_size_u) {
    errno = EINVAL;
    return -1;
  }
  uintptr_t end = last_page + page_size_u;
  uintptr_t lock_size = end - start;
  if (lock_size > (uintptr_t)SIZE_MAX) {
    errno = EINVAL;
    return -1;
  }
  return mlock((void *)start, (size_t)lock_size);
#elif HAVE_MLOCKALL
  return mlockall(MCL_CURRENT);
#else
  errno = ENOSYS;
  return -1;
#endif
}

#define MLOCK_PAGE(ptr, size) MlockPage((ptr), (size))

#endif
