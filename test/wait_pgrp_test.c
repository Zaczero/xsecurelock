#include <assert.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../wait_pgrp.h"

pid_t waitpid(pid_t pid, int *status, int options) {
  (void)pid;
  (void)status;
  (void)options;
  errno = EINVAL;
  return -1;
}

int main(void) {
  pid_t pid = 123;
  int exit_status = 456;

  assert(WaitProc("test", &pid, 1, 0, &exit_status) == 0);
  assert(pid == 123);
  assert(exit_status == 456);
  return 0;
}
