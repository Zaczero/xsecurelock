#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../env_settings.h"

static void ExpectUnsetFallsBackToDefault(void) {
  unsetenv("XSECURELOCK_TEST_INT");
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", 12, 0, 100) != 12) {
    abort();
  }

  unsetenv("XSECURELOCK_TEST_UNSIGNED");
  if (GetUnsignedLongLongSetting("XSECURELOCK_TEST_UNSIGNED", 99) != 99) {
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

  setenv("XSECURELOCK_TEST_INT", "-5", 1);
  if (GetClampedIntSetting("XSECURELOCK_TEST_INT", 2048, 1, 1 << 30) != 1) {
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

static void ExpectUnsignedSettingRejectsNegativeInput(void) {
  setenv("XSECURELOCK_TEST_UNSIGNED", "-1", 1);
  if (GetUnsignedLongLongSetting("XSECURELOCK_TEST_UNSIGNED", 99) != 99) {
    abort();
  }

  setenv("XSECURELOCK_TEST_UNSIGNED", "42", 1);
  if (GetUnsignedLongLongSetting("XSECURELOCK_TEST_UNSIGNED", 99) != 42) {
    abort();
  }
}

static void ExpectUnsignedSettingRejectsOverflow(void) {
  char overflow[64];

  if (snprintf(overflow, sizeof(overflow), "%llu0", ULLONG_MAX) <= 0) {
    abort();
  }
  setenv("XSECURELOCK_TEST_UNSIGNED", overflow, 1);
  if (GetUnsignedLongLongSetting("XSECURELOCK_TEST_UNSIGNED", 99) != 99) {
    abort();
  }
}

static void ExpectBoolSettingsNormalize(void) {
  unsetenv("XSECURELOCK_TEST_BOOL");
  if (GetBoolSetting("XSECURELOCK_TEST_BOOL", 7) != 1) {
    abort();
  }

  setenv("XSECURELOCK_TEST_BOOL", "0", 1);
  if (GetBoolSetting("XSECURELOCK_TEST_BOOL", 1) != 0) {
    abort();
  }

  setenv("XSECURELOCK_TEST_BOOL", "7", 1);
  if (GetBoolSetting("XSECURELOCK_TEST_BOOL", 0) != 1) {
    abort();
  }

  setenv("XSECURELOCK_TEST_BOOL", "nope", 1);
  if (GetBoolSetting("XSECURELOCK_TEST_BOOL", 0) != 0) {
    abort();
  }
}

static void ExpectNonnegativeSettingsClamp(void) {
  setenv("XSECURELOCK_TEST_NONNEGATIVE_INT", "-5", 1);
  if (GetNonnegativeIntSetting("XSECURELOCK_TEST_NONNEGATIVE_INT", 12) != 0) {
    abort();
  }

  setenv("XSECURELOCK_TEST_NONNEGATIVE_INT", "37", 1);
  if (GetNonnegativeIntSetting("XSECURELOCK_TEST_NONNEGATIVE_INT", 12) != 37) {
    abort();
  }
}

static void ExpectPositiveSettingsClamp(void) {
  setenv("XSECURELOCK_TEST_POSITIVE_INT", "-5", 1);
  if (GetPositiveIntSetting("XSECURELOCK_TEST_POSITIVE_INT", 12) != 1) {
    abort();
  }

  setenv("XSECURELOCK_TEST_POSITIVE_INT", "0", 1);
  if (GetPositiveIntSetting("XSECURELOCK_TEST_POSITIVE_INT", 12) != 1) {
    abort();
  }

  setenv("XSECURELOCK_TEST_POSITIVE_INT", "37", 1);
  if (GetPositiveIntSetting("XSECURELOCK_TEST_POSITIVE_INT", 12) != 37) {
    abort();
  }
}

static void ExpectIntSettingRejectsOverflow(void) {
  char overflow[64];

  if (snprintf(overflow, sizeof(overflow), "%ld0", LONG_MAX) <= 0) {
    abort();
  }
  setenv("XSECURELOCK_TEST_INT", overflow, 1);
  if (GetIntSetting("XSECURELOCK_TEST_INT", 12) != 12) {
    abort();
  }
}

static void ExpectFiniteDoubleSettingRejectsNanAndInfinity(void) {
  setenv("XSECURELOCK_TEST_DOUBLE", "3.5", 1);
  if (GetFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25) != 3.5) {
    abort();
  }

  setenv("XSECURELOCK_TEST_DOUBLE", "nan", 1);
  if (GetFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25) != 1.25) {
    abort();
  }

  setenv("XSECURELOCK_TEST_DOUBLE", "inf", 1);
  if (GetFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25) != 1.25) {
    abort();
  }

  setenv("XSECURELOCK_TEST_DOUBLE", "-inf", 1);
  if (GetFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25) != 1.25) {
    abort();
  }
}

static void ExpectClampedFiniteDoubleSettingClampsAndFallsBack(void) {
  setenv("XSECURELOCK_TEST_DOUBLE", "-5", 1);
  if (GetClampedFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25, 0.5,
                                    4.0) != 0.5) {
    abort();
  }

  setenv("XSECURELOCK_TEST_DOUBLE", "8", 1);
  if (GetClampedFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25, 0.5,
                                    4.0) != 4.0) {
    abort();
  }

  setenv("XSECURELOCK_TEST_DOUBLE", "oops", 1);
  if (fabs(GetClampedFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", 1.25, 0.5,
                                         4.0) -
           1.25) > 0.000001) {
    abort();
  }

  unsetenv("XSECURELOCK_TEST_DOUBLE");
  if (GetClampedFiniteDoubleSetting("XSECURELOCK_TEST_DOUBLE", -2.0, 0.5,
                                    4.0) != 0.5) {
    abort();
  }
}

int main(void) {
  ExpectUnsetFallsBackToDefault();
  ExpectValuesClampToBounds();
  ExpectInRangeValuePassesThrough();
  ExpectInvalidValueFallsBackThenClampsDefault();
  ExpectUnsignedSettingRejectsNegativeInput();
  ExpectUnsignedSettingRejectsOverflow();
  ExpectBoolSettingsNormalize();
  ExpectNonnegativeSettingsClamp();
  ExpectPositiveSettingsClamp();
  ExpectIntSettingRejectsOverflow();
  ExpectFiniteDoubleSettingRejectsNanAndInfinity();
  ExpectClampedFiniteDoubleSettingClampsAndFallsBack();
  unsetenv("XSECURELOCK_TEST_INT");
  unsetenv("XSECURELOCK_TEST_BOOL");
  unsetenv("XSECURELOCK_TEST_NONNEGATIVE_INT");
  unsetenv("XSECURELOCK_TEST_POSITIVE_INT");
  unsetenv("XSECURELOCK_TEST_DOUBLE");
  unsetenv("XSECURELOCK_TEST_UNSIGNED");
  return 0;
}
