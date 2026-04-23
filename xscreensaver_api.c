/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"

#include "xscreensaver_api.h"

#include <X11/X.h>   // for Window
#include <stdio.h>   // for fprintf, stderr
#include <stdlib.h>  // for getenv, setenv

#include "env_settings.h"  // for GetUnsignedLongLongSetting
#include "logging.h"

void ExportWindowID(Window w) {
  char window_id_str[32];
  int window_id_len = snprintf(window_id_str, sizeof(window_id_str), "%llu",
                               (unsigned long long)w);
  if (window_id_len <= 0 || (size_t)window_id_len >= sizeof(window_id_str)) {
    Log("Window ID doesn't fit into buffer");
    return;
  }
  if (setenv("XSCREENSAVER_WINDOW", window_id_str, 1) != 0) {
    LogErrno("setenv XSCREENSAVER_WINDOW");
  }
}

void ExportSaverIndex(int index) {
  char saver_index_str[32];
  int saver_index_len =
      snprintf(saver_index_str, sizeof(saver_index_str), "%d", index);
  if (saver_index_len <= 0 ||
      (size_t)saver_index_len >= sizeof(saver_index_str)) {
    Log("Saver index doesn't fit into buffer");
    return;
  }
  if (setenv("XSCREENSAVER_SAVER_INDEX", saver_index_str, 1) != 0) {
    LogErrno("setenv XSCREENSAVER_SAVER_INDEX");
  }
}

Window ReadWindowID(void) {
  unsigned long long raw =
      GetUnsignedLongLongSetting("XSCREENSAVER_WINDOW", None);
  Window window = (Window)raw;
  if ((unsigned long long)window != raw) {
    const char *value = getenv("XSCREENSAVER_WINDOW");

    Log("Ignoring out-of-range value of XSCREENSAVER_WINDOW: %s",
        value != NULL ? value : "(null)");
    return None;
  }
  return window;
}
