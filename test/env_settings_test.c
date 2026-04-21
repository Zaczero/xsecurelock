#include <limits.h>
#include <stdlib.h>

#include "../env_settings.h"

static void ExpectUnsetFallsBackToDefault(void) {
  unsetenv("XSECURELOCK_TEST_INT");
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", 12, 0, 100) != 12) {
    abort();
  }
}

static void ExpectValuesClampToBounds(void) {
  setenv("XSECURELOCK_TEST_INT", "-5", 1);
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", 12, 0, 100) != 0) {
    abort();
  }

  setenv("XSECURELOCK_TEST_INT", "105", 1);
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", 12, 0, 100) != 100) {
    abort();
  }
}

static void ExpectInRangeValuePassesThrough(void) {
  setenv("XSECURELOCK_TEST_INT", "37", 1);
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", 12, 0, 100) != 37) {
    abort();
  }
}

static void ExpectInvalidValueFallsBackThenClampsDefault(void) {
  setenv("XSECURELOCK_TEST_INT", "not-a-number", 1);
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", -3, 0, 100) != 0) {
    abort();
  }
}

int main(void) {
  ExpectUnsetFallsBackToDefault();
  ExpectValuesClampToBounds();
  ExpectInRangeValuePassesThrough();
  ExpectInvalidValueFallsBackThenClampsDefault();
  unsetenv("XSECURELOCK_TEST_INT");
  return 0;
}
