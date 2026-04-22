#include <stdlib.h>
#include <string.h>

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

static void ExpectSaverIndexUsesSignedFormatting(void) {
  const char *value = NULL;

  unsetenv("XSCREENSAVER_SAVER_INDEX");
  ExportSaverIndex(-1);
  value = getenv("XSCREENSAVER_SAVER_INDEX");
  if (value == NULL || strcmp(value, "-1") != 0) {
    abort();
  }
}

int main(void) {
  ExpectUnsetFallsBackToNone();
  ExpectRoundTripWindowID();
  ExpectOverflowFallsBackToNone();
  ExpectSaverIndexUsesSignedFormatting();
  unsetenv("XSCREENSAVER_WINDOW");
  unsetenv("XSCREENSAVER_SAVER_INDEX");
  return 0;
}
