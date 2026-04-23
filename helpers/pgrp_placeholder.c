/*
 * Copyright 2019 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*!
 *\brief Process group placeholder.
 *
 * Does nothing except sitting around until killed. Spawned as extra process in
 * our process groups so that we can control on our own when the process group
 * ID is reclaimed to the kernel, namely by killing the entire process group.
 * This prevents a race condition of our process group getting reclaimed before
 * we try to kill possibly remaining processes in it, after which we would
 * possibly kill something else.
 *
 * Must be a separate executable so F_CLOEXEC applies as intended.
 */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>

int main(void) {
  for (;;) {
    pause();
  }
  // Cannot get here.
  abort();
}
