#include <stdlib.h>

#include <X11/X.h>

#include "../xscreensaver_api.h"

static void ExpectUnsetFallsBackToNone(void) {
  unsetenv("XSCREENSAVER_WINDOW");
  if (ReadWindowID() != None) {
    abort();
  }
}

static void ExpectRoundTripWindowID(void) {
  Window window = (Window)12345;

  unsetenv("XSCREENSAVER_WINDOW");
  ExportWindowID(window);
  if (ReadWindowID() != window) {
    abort();
  }
}

static void ExpectOverflowFallsBackToNone(void) {
  if (sizeof(Window) >= sizeof(unsigned long long)) {
    return;
  }

  setenv("XSCREENSAVER_WINDOW", "4294967296", 1);
  if (ReadWindowID() != None) {
    abort();
  }
}

int main(void) {
  ExpectUnsetFallsBackToNone();
  ExpectRoundTripWindowID();
  ExpectOverflowFallsBackToNone();
  unsetenv("XSCREENSAVER_WINDOW");
  return 0;
}
