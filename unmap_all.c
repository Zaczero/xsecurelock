#include "config.h"

#include "unmap_all.h"

#include <X11/X.h>      // for AnyPropertyType, Atom, None, Success, Window
#include <X11/Xlib.h>   // for XFree, XGetWindowAttributes, XGetWindowProperty
#include <X11/Xutil.h>  // for XClassHint, XGetClassHint
#include <string.h>     // for NULL, strcmp

enum { FIND_CLIENT_WINDOW_MAX_DEPTH = 64 };

static int HasWindowProperty(Display *display, Window window, Atom property) {
  Atom actual_type = None;
  int actual_format = 0;
  unsigned long nitems = 0;
  unsigned long bytes_after = 0;
  unsigned char *data = NULL;
  int status = XGetWindowProperty(display, window, property, 0, 0, False,
                                  AnyPropertyType, &actual_type, &actual_format,
                                  &nitems, &bytes_after, &data);
  if (data != NULL) {
    XFree(data);
  }
  return status == Success && actual_type != None;
}

static Window FindClientWindowAtDepth(Display *display, Window window,
                                      Atom wm_state, int depth) {
  if (HasWindowProperty(display, window, wm_state)) {
    return window;
  }
  if (depth >= FIND_CLIENT_WINDOW_MAX_DEPTH) {
    return None;
  }

  Window root_return = None;
  Window parent_return = None;
  Window *children = NULL;
  unsigned int nchildren = 0;
  if (!XQueryTree(display, window, &root_return, &parent_return, &children,
                  &nchildren)) {
    return None;
  }

  Window found = None;
  for (unsigned int i = 0; i < nchildren; ++i) {
    found = FindClientWindowAtDepth(display, children[i], wm_state, depth + 1);
    if (found != None) {
      break;
    }
  }
  if (children != NULL) {
    XFree(children);
  }
  return found;
}

static Window FindClientWindow(Display *display, Window window, Atom wm_state) {
  if (wm_state == None) {
    return window;
  }

  Window client = FindClientWindowAtDepth(display, window, wm_state, 0);
  return client != None ? client : window;
}

int InitUnmapAllWindowsState(UnmapAllWindowsState *state, Display *display,
                             Window root_window, const Window *ignored_windows,
                             size_t n_ignored_windows, const char *my_res_class,
                             const char *my_res_name, int include_frame) {
  int should_proceed = 1;
  state->display = display;
  state->root_window = root_window;
  state->windows = NULL;
  state->n_windows = 0;

  Window unused_root_return, unused_parent_return;
  if (!XQueryTree(state->display, state->root_window, &unused_root_return,
                  &unused_parent_return, &state->windows, &state->n_windows)) {
    state->windows = NULL;
    state->n_windows = 0;
    state->first_unmapped_window = 0;
    return 0;
  }
  state->first_unmapped_window = state->n_windows;  // That means none unmapped.
  Atom wm_state = include_frame ? None : XInternAtom(display, "WM_STATE", True);
  for (unsigned int i = 0; i < state->n_windows; ++i) {
    XWindowAttributes xwa;
    if (!XGetWindowAttributes(display, state->windows[i], &xwa)) {
      state->windows[i] = None;
      continue;
    }
    // Not mapped -> nothing to do.
    if (xwa.map_state == IsUnmapped) {
      state->windows[i] = None;
      continue;
    }
    // Go down to the next WM_STATE window if available, as unmapping window
    // frames may confuse WMs.
    if (!include_frame) {
      state->windows[i] =
          FindClientWindow(display, state->windows[i], wm_state);
    }
    // If any window we'd be unmapping is in the ignore list, skip it.
    for (size_t j = 0; j < n_ignored_windows; ++j) {
      if (state->windows[i] == ignored_windows[j]) {
        state->windows[i] = None;
      }
    }
    if (state->windows[i] == None) {
      continue;
    }
    XClassHint cls;
    if (XGetClassHint(state->display, state->windows[i], &cls)) {
      const char *res_class = cls.res_class != NULL ? cls.res_class : "";
      const char *res_name = cls.res_name != NULL ? cls.res_name : "";
      // If any window has my window class, we better not proceed with
      // unmapping as doing so could accidentally unlock the screen or
      // otherwise cause more damage than good.
      if ((my_res_class || my_res_name) &&
          (!my_res_class || strcmp(my_res_class, res_class) == 0) &&
          (!my_res_name || strcmp(my_res_name, res_name) == 0)) {
        state->windows[i] = None;
        should_proceed = 0;
      }
      // HACK: Bspwm creates some subwindows of the root window that we
      // absolutely shouldn't ever unmap, as remapping them confuses Bspwm.
      if (!strcmp(res_class, "Bspwm")) {
        state->windows[i] = None;
      }
      XFree(cls.res_class);
      cls.res_class = NULL;
      XFree(cls.res_name);
      cls.res_name = NULL;
    }
  }
  return should_proceed;
}

int UnmapAllWindows(UnmapAllWindowsState *state,
                    int (*just_unmapped_can_we_stop)(Window w, void *arg),
                    void *arg) {
  if (state->first_unmapped_window == 0) {  // Already all unmapped.
    return 0;
  }
  for (unsigned int i = state->first_unmapped_window - 1;;
       --i) {  // Top-to-bottom order!
    if (state->windows[i] != None) {
      XUnmapWindow(state->display, state->windows[i]);
      state->first_unmapped_window = i;
      int ret;
      if (just_unmapped_can_we_stop != NULL &&
          (ret = just_unmapped_can_we_stop(state->windows[i], arg))) {
        return ret;
      }
    }
    if (i == 0) {
      return 0;
    }
  }
}

void RemapAllWindows(UnmapAllWindowsState *state) {
  for (unsigned int i = state->first_unmapped_window; i < state->n_windows;
       ++i) {
    if (state->windows[i] != None) {
      XMapWindow(state->display, state->windows[i]);
    }
  }
  state->first_unmapped_window = state->n_windows;
}

void ClearUnmapAllWindowsState(UnmapAllWindowsState *state) {
  state->display = NULL;
  state->root_window = None;
  XFree(state->windows);
  state->windows = NULL;
  state->n_windows = 0;
  state->first_unmapped_window = 0;
}
