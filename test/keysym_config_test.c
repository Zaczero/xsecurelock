#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "../keysym_config.h"

static void ExpectUnsetUsesDefault(void) {
  KeySym keysym = NoSymbol;
  unsetenv("XSECURELOCK_TEST_KEYSYM");

  const char *name =
      GetKeySymSetting("XSECURELOCK_TEST_KEYSYM", "Tab", &keysym);
  if (keysym != XK_Tab || strcmp(name, "Tab") != 0) {
    abort();
  }
}

static void ExpectValidLowercaseKeySym(void) {
  KeySym keysym = NoSymbol;
  setenv("XSECURELOCK_TEST_KEYSYM", "space", 1);

  const char *name =
      GetKeySymSetting("XSECURELOCK_TEST_KEYSYM", "Tab", &keysym);
  if (keysym != XK_space || strcmp(name, "space") != 0) {
    abort();
  }
}

static void ExpectValidCanonicalKeySym(void) {
  KeySym keysym = NoSymbol;
  setenv("XSECURELOCK_TEST_KEYSYM", "ISO_Left_Tab", 1);

  const char *name =
      GetKeySymSetting("XSECURELOCK_TEST_KEYSYM", "Tab", &keysym);
  if (keysym != XK_ISO_Left_Tab || strcmp(name, "ISO_Left_Tab") != 0) {
    abort();
  }
}

static void ExpectInvalidFallsBackToDefault(void) {
  KeySym keysym = NoSymbol;
  setenv("XSECURELOCK_TEST_KEYSYM", "not-a-keysym", 1);

  const char *name =
      GetKeySymSetting("XSECURELOCK_TEST_KEYSYM", "Tab", &keysym);
  if (keysym != XK_Tab || strcmp(name, "Tab") != 0) {
    abort();
  }
}

static void ExpectEmptyFallsBackToDefault(void) {
  KeySym keysym = NoSymbol;
  setenv("XSECURELOCK_TEST_KEYSYM", "", 1);

  const char *name =
      GetKeySymSetting("XSECURELOCK_TEST_KEYSYM", "Tab", &keysym);
  if (keysym != XK_Tab || strcmp(name, "Tab") != 0) {
    abort();
  }
}

int main(void) {
  ExpectUnsetUsesDefault();
  ExpectValidLowercaseKeySym();
  ExpectValidCanonicalKeySym();
  ExpectInvalidFallsBackToDefault();
  ExpectEmptyFallsBackToDefault();
  unsetenv("XSECURELOCK_TEST_KEYSYM");
  return 0;
}
