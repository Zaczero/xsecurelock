#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "../mlock_page.h"

int main(void) {
#if HAVE_MLOCK && !FORCE_MLOCK_PAGE_UNAVAILABLE
  errno = 0;
  assert(MLOCK_PAGE((const void *)(uintptr_t)UINTPTR_MAX, 2) == -1);
  assert(errno == EINVAL);
#endif
  return 0;
}
