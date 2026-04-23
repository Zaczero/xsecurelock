#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xmu/WinUtil.h>
#include <X11/Xutil.h>

#include "../unmap_all.h"

enum { MAX_FAKE_WINDOWS = 8 };

struct FakeWindowInfo {
  Window window;
  int attributes_ok;
  int map_state;
  Window client_window;
  int have_class_hint;
  const char *res_class;
  const char *res_name;
};

static struct {
  int query_tree_ok;
  Window query_windows[MAX_FAKE_WINDOWS];
  unsigned int query_window_count;
  struct FakeWindowInfo windows[MAX_FAKE_WINDOWS];
  size_t window_count;
  Window unmapped[MAX_FAKE_WINDOWS];
  size_t unmapped_count;
  Window mapped[MAX_FAKE_WINDOWS];
  size_t mapped_count;
} g_state;

static char *DuplicateOrNull(const char *value) {
  if (value == NULL) {
    return NULL;
  }

  size_t len = strlen(value);
  char *copy = malloc(len + 1);
  assert(copy != NULL);
  memcpy(copy, value, len + 1);
  return copy;
}

static void ResetFakeState(void) {
  memset(&g_state, 0, sizeof(g_state));
  g_state.query_tree_ok = 1;
}

static struct FakeWindowInfo *FindWindow(Window window) {
  for (size_t i = 0; i < g_state.window_count; ++i) {
    if (g_state.windows[i].window == window) {
      return &g_state.windows[i];
    }
  }
  return NULL;
}

static void AddFakeWindow(Window window, int attributes_ok, int map_state,
                          Window client_window, int have_class_hint,
                          const char *res_class, const char *res_name) {
  assert(g_state.window_count < MAX_FAKE_WINDOWS);
  assert(g_state.query_window_count < MAX_FAKE_WINDOWS);

  g_state.query_windows[g_state.query_window_count++] = window;
  g_state.windows[g_state.window_count++] = (struct FakeWindowInfo){
      .window = window,
      .attributes_ok = attributes_ok,
      .map_state = map_state,
      .client_window = client_window,
      .have_class_hint = have_class_hint,
      .res_class = res_class,
      .res_name = res_name,
  };
}

Status XQueryTree(Display *display, Window w, Window *root_return,
                  Window *parent_return, Window **children_return,
                  unsigned int *nchildren_return) {
  (void)display;
  (void)w;
  if (!g_state.query_tree_ok) {
    return 0;
  }

  *root_return = None;
  *parent_return = None;
  *nchildren_return = g_state.query_window_count;
  if (g_state.query_window_count == 0) {
    *children_return = NULL;
  } else {
    *children_return = malloc(sizeof(Window) * g_state.query_window_count);
    assert(*children_return != NULL);
    memcpy(*children_return, g_state.query_windows,
           sizeof(Window) * g_state.query_window_count);
  }
  return 1;
}

Status XGetWindowAttributes(Display *display, Window w,
                            XWindowAttributes *window_attributes_return) {
  (void)display;
  struct FakeWindowInfo *info = FindWindow(w);
  assert(info != NULL);
  if (!info->attributes_ok) {
    return 0;
  }

  memset(window_attributes_return, 0, sizeof(*window_attributes_return));
  window_attributes_return->map_state = info->map_state;
  return 1;
}

Window XmuClientWindow(Display *display, Window window) {
  (void)display;
  struct FakeWindowInfo *info = FindWindow(window);
  assert(info != NULL);
  return info->client_window != None ? info->client_window : window;
}

Status XGetClassHint(Display *display, Window w, XClassHint *class_hints_return) {
  (void)display;
  struct FakeWindowInfo *info = FindWindow(w);
  assert(info != NULL);
  if (!info->have_class_hint) {
    return 0;
  }

  class_hints_return->res_class = DuplicateOrNull(info->res_class);
  class_hints_return->res_name = DuplicateOrNull(info->res_name);
  return 1;
}

int XUnmapWindow(Display *display, Window w) {
  (void)display;
  assert(g_state.unmapped_count < MAX_FAKE_WINDOWS);
  g_state.unmapped[g_state.unmapped_count++] = w;
  return 0;
}

int XMapWindow(Display *display, Window w) {
  (void)display;
  assert(g_state.mapped_count < MAX_FAKE_WINDOWS);
  g_state.mapped[g_state.mapped_count++] = w;
  return 0;
}

int XFree(void *data) {
  free(data);
  return 0;
}

static void ExpectQueryTreeFailureLeavesEmptyState(void) {
  ResetFakeState();
  g_state.query_tree_ok = 0;

  UnmapAllWindowsState state;
  memset(&state, 0xAB, sizeof(state));
  assert(InitUnmapAllWindowsState(&state, NULL, None, NULL, 0, NULL, NULL, 0) ==
         0);
  assert(state.windows == NULL);
  assert(state.n_windows == 0);
  assert(state.first_unmapped_window == 0);
  ClearUnmapAllWindowsState(&state);
}

static void ExpectAttributeFailureSkipsWindow(void) {
  ResetFakeState();
  AddFakeWindow((Window)11, 0, IsViewable, None, 0, NULL, NULL);

  UnmapAllWindowsState state;
  assert(InitUnmapAllWindowsState(&state, NULL, None, NULL, 0, NULL, NULL, 0) ==
         1);
  assert(state.n_windows == 1);
  assert(state.windows[0] == None);
  ClearUnmapAllWindowsState(&state);
}

static void ExpectNullClassNameDoesNotCrashAndBspwmIgnored(void) {
  ResetFakeState();
  AddFakeWindow((Window)21, 1, IsViewable, None, 1, NULL, NULL);
  AddFakeWindow((Window)22, 1, IsViewable, None, 1, "Bspwm", NULL);

  UnmapAllWindowsState state;
  assert(InitUnmapAllWindowsState(&state, NULL, None, NULL, 0, "Mine", "App",
                                  0) == 1);
  assert(state.n_windows == 2);
  assert(state.windows[0] == (Window)21);
  assert(state.windows[1] == None);
  ClearUnmapAllWindowsState(&state);
}

static void ExpectOwnClassStopsProceeding(void) {
  ResetFakeState();
  AddFakeWindow((Window)31, 1, IsViewable, None, 1, "Mine", "App");

  UnmapAllWindowsState state;
  assert(InitUnmapAllWindowsState(&state, NULL, None, NULL, 0, "Mine", "App",
                                  0) == 0);
  assert(state.windows[0] == None);
  ClearUnmapAllWindowsState(&state);
}

static void ExpectUnmapAndRemapRespectTrackedWindows(void) {
  ResetFakeState();
  AddFakeWindow((Window)41, 1, IsViewable, None, 0, NULL, NULL);
  AddFakeWindow((Window)42, 1, IsUnmapped, None, 0, NULL, NULL);
  AddFakeWindow((Window)43, 1, IsViewable, None, 0, NULL, NULL);

  UnmapAllWindowsState state;
  assert(InitUnmapAllWindowsState(&state, NULL, None, NULL, 0, NULL, NULL, 0) ==
         1);
  assert(UnmapAllWindows(&state, NULL, NULL) == 0);
  assert(g_state.unmapped_count == 2);
  assert(g_state.unmapped[0] == (Window)43);
  assert(g_state.unmapped[1] == (Window)41);

  RemapAllWindows(&state);
  assert(g_state.mapped_count == 2);
  assert(g_state.mapped[0] == (Window)41);
  assert(g_state.mapped[1] == (Window)43);

  ClearUnmapAllWindowsState(&state);
}

int main(void) {
  ExpectQueryTreeFailureLeavesEmptyState();
  ExpectAttributeFailureSkipsWindow();
  ExpectNullClassNameDoesNotCrashAndBspwmIgnored();
  ExpectOwnClassStopsProceeding();
  ExpectUnmapAndRemapRespectTrackedWindows();
  return 0;
}
