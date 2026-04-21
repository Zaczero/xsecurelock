#include <assert.h>
#include <errno.h>

#include "../mlock_page.h"

int main(void) {
  char byte = 0;

  errno = 0;
  assert(MLOCK_PAGE(&byte, 0) == 0);
  assert(errno == 0);

  errno = 0;
  assert(MLOCK_PAGE(&byte, 1) == -1);
  assert(errno == ENOSYS);

  return 0;
}
