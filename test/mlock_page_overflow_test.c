#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "../mlock_page.h"

int main(void) {
#if HAVE_MLOCK && !FORCE_MLOCK_PAGE_UNAVAILABLE
  errno = 0;
  assert(MLOCK_PAGE((const void *)(uintptr_t)UINTPTR_MAX, 2) == -1);
  assert(errno == EINVAL);

  errno = 0;
  assert(MLOCK_PAGE((const void *)(uintptr_t)(UINTPTR_MAX - 1), 1) == -1);
  assert(errno == EINVAL);

#if SIZE_MAX < UINTPTR_MAX
  errno = 0;
  assert(MLOCK_PAGE((const void *)(uintptr_t)1, SIZE_MAX) == -1);
  assert(errno == EINVAL);
#endif
#endif
  return 0;
}
