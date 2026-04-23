/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"

#include "wm_properties.h"

#include <X11/Xutil.h>  // for XClassHint, XSetWMProperties
#include <string.h>     // for memcpy

static char *BorrowMutableString(const char *str) {
  char *mutable_str = NULL;
  memcpy(&mutable_str, &str, sizeof(mutable_str));
  return mutable_str;
}

static char **BorrowMutableArgv(char *const *argv) {
  char **mutable_argv = NULL;
  memcpy(&mutable_argv, &argv, sizeof(mutable_argv));
  return mutable_argv;
}

void SetWMProperties(Display* dpy, Window w, const char* res_class,
                     const char* res_name, int argc, char* const* argv) {
  XClassHint class_hint = {
      .res_name = BorrowMutableString(res_name),
      .res_class = BorrowMutableString(res_class),
  };

  XTextProperty name_prop;
  char* window_names[] = {BorrowMutableString(res_name)};
  if (!XStringListToTextProperty(window_names, 1, &name_prop)) {
    return;
  }

  // XSetWMProperties copies these caller-owned strings into X11 properties.
  XSetWMProperties(dpy, w, &name_prop, &name_prop, BorrowMutableArgv(argv),
                   argc, NULL, NULL, &class_hint);
  XFree(name_prop.value);
}
