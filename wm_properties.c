/*
Copyright 2018 Google Inc. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/

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
