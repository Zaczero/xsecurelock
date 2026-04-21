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

#include <X11/Xutil.h>  // for XClassHint, XAllocClassHint, XSetWMProperties
#include <stddef.h>     // for NULL, size_t
#include <stdlib.h>     // for free, malloc
#include <string.h>     // for memcpy, strdup

void SetWMProperties(Display* dpy, Window w, const char* res_class,
                     const char* res_name, int argc, char* const* argv) {
  XClassHint* class_hint = XAllocClassHint();
  if (class_hint == NULL) {
    return;
  }

  char* res_name_copy = strdup(res_name);
  char* res_class_copy = strdup(res_class);
  if (res_name_copy == NULL || res_class_copy == NULL) {
    free(res_name_copy);
    free(res_class_copy);
    XFree(class_hint);
    return;
  }

  char** argv_copy = NULL;
  if (argc > 0 && argv != NULL) {
    argv_copy = malloc((size_t)argc * sizeof(*argv_copy));
    if (argv_copy == NULL) {
      free(res_name_copy);
      free(res_class_copy);
      XFree(class_hint);
      return;
    }
    memcpy(argv_copy, argv, (size_t)argc * sizeof(*argv_copy));
  }

  class_hint->res_name = res_name_copy;
  class_hint->res_class = res_class_copy;
  XTextProperty name_prop;
  char* window_names[] = {res_name_copy};
  if (!XStringListToTextProperty(window_names, 1, &name_prop)) {
    free(argv_copy);
    free(res_name_copy);
    free(res_class_copy);
    XFree(class_hint);
    return;
  }
  XSetWMProperties(dpy, w, &name_prop, &name_prop, argv_copy, argc, NULL, NULL,
                   class_hint);
  XFree(name_prop.value);
  free(argv_copy);
  free(res_name_copy);
  free(res_class_copy);
  XFree(class_hint);
}
