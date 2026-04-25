/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "config.h"

#include "env_settings.h"

#include <assert.h>  // for assert
#include <errno.h>   // for errno, ENAMETOOLONG, ERANGE
#include <limits.h>  // for INT_MAX
#include <math.h>    // for isfinite
#include <stdint.h>  // for SIZE_MAX
#include <stdlib.h>  // for free, getenv, malloc, strtol, strtoull
#include <string.h>  // for memcpy, strchr, strlen
#include <unistd.h>  // for access, X_OK

#include "build-config.h"
#include "logging.h"

static char *JoinHelperPath(const char *name) {
  size_t helper_len = strlen(HELPER_PATH);
  size_t name_len = strlen(name);

  if (helper_len > SIZE_MAX - 1 || helper_len + 1 > SIZE_MAX - name_len ||
      helper_len + 1 + name_len > SIZE_MAX - 1) {
    errno = ENAMETOOLONG;
    return NULL;
  }

  char *path = malloc(helper_len + 1 + name_len + 1);
  if (path == NULL) {
    return NULL;
  }

  memcpy(path, HELPER_PATH, helper_len + 1);
  path[helper_len] = '/';
  memcpy(path + helper_len + 1, name, name_len + 1);
  return path;
}

unsigned long long GetUnsignedLongLongSetting(const char *name,
                                              unsigned long long def) {
  const char *value = getenv(name);
  if (value == NULL || value[0] == 0) {
    return def;
  }
  if (value[0] == '-') {
    Log("Ignoring negative value of %s: %s", name, value);
    return def;
  }
  char *endptr = NULL;
  errno = 0;
  unsigned long long number = strtoull(value, &endptr, 0);
  if (errno == ERANGE) {
    Log("Ignoring out-of-range value of %s: %s", name, value);
    return def;
  }
  if ((endptr != NULL && *endptr != 0)) {
    Log("Ignoring non-numeric value of %s: %s", name, value);
    return def;
  }
  return number;
}

long GetLongSetting(const char *name, long def) {
  const char *value = getenv(name);
  if (value == NULL || value[0] == 0) {
    return def;
  }
  char *endptr = NULL;
  errno = 0;
  long number = strtol(value, &endptr, 0);
  if (errno == ERANGE) {
    Log("Ignoring out-of-range value of %s: %s", name, value);
    return def;
  }
  if ((endptr != NULL && *endptr != 0)) {
    Log("Ignoring non-numeric value of %s: %s", name, value);
    return def;
  }
  return number;
}

int GetIntSetting(const char *name, int def) {
  long lnumber = GetLongSetting(name, def);
  int number = (int)lnumber;
  if (lnumber != (long)number) {
    const char *value = getenv(name);
    Log("Ignoring out-of-range value of %s: %s", name,
        value != NULL ? value : "(default)");
    return def;
  }
  return number;
}

int GetClampedIntSetting(const char *name, int def, int min_value,
                         int max_value) {
  int number = GetIntSetting(name, def);
  if (number < min_value) {
    return min_value;
  }
  if (number > max_value) {
    return max_value;
  }
  return number;
}

int GetBoolSetting(const char *name, int def) {
  return GetIntSetting(name, def) != 0;
}

int GetNonnegativeIntSetting(const char *name, int def) {
  return GetClampedIntSetting(name, def, 0, INT_MAX);
}

int GetPositiveIntSetting(const char *name, int def) {
  return GetClampedIntSetting(name, def, 1, INT_MAX);
}

double GetDoubleSetting(const char *name, double def) {
  const char *value = getenv(name);
  if (value == NULL || value[0] == 0) {
    return def;
  }
  char *endptr = NULL;
  errno = 0;
  double number = strtod(value, &endptr);
  if (errno == ERANGE) {
    Log("Ignoring out-of-range value of %s: %s", name, value);
    return def;
  }
  if ((endptr != NULL && *endptr != 0)) {
    Log("Ignoring non-numeric value of %s: %s", name, value);
    return def;
  }
  return number;
}

double GetFiniteDoubleSetting(const char *name, double def) {
  double number = GetDoubleSetting(name, def);
  if (!isfinite(number)) {
    const char *value = getenv(name);
    Log("Ignoring non-finite value of %s: %s", name,
        value != NULL ? value : "(default)");
    return def;
  }
  return number;
}

double GetClampedFiniteDoubleSetting(const char *name, double def,
                                     double min_value, double max_value) {
  assert(min_value <= max_value);
  double number = GetFiniteDoubleSetting(name, def);
  if (number < min_value) {
    return min_value;
  }
  if (number > max_value) {
    return max_value;
  }
  return number;
}

const char *GetStringSetting(const char *name, const char *def) {
  const char *value = getenv(name);
  if (value == NULL || value[0] == 0) {
    return def;
  }
  return value;
}

const char *GetExecutablePathSetting(const char *name, const char *def,
                                     int is_auth) {
  const char *value = getenv(name);
  if (value == NULL || value[0] == 0) {
    return def;
  }
  if (strchr(value, '/') && value[0] != '/') {
    Log("Executable name '%s' must be either an absolute path or a file within "
        "%s",
        value, HELPER_PATH);
    return def;
  }
  const char *basename = strrchr(value, '/');
  if (basename == NULL) {
    basename = value;  // No slash, use as is.
  } else {
    ++basename;  // Skip the slash.
  }
  if (is_auth) {
    if (strncmp(basename, "auth_", 5) != 0) {
      Log("Auth executable name '%s' must start with auth_", value);
      return def;
    }
  } else {
    if (!strncmp(basename, "auth_", 5)) {
      Log("Non-auth executable name '%s' must not start with auth_", value);
      return def;
    }
  }
  const char *executable_path = value;
  char *helper_path = NULL;
  if (basename == value) {
    helper_path = JoinHelperPath(value);
    if (helper_path == NULL) {
      LogErrno("Unable to build executable path '%s/%s'", HELPER_PATH, value);
      return def;
    }
    executable_path = helper_path;
  }
  if (access(executable_path, X_OK)) {
    Log("Executable '%s' must be executable", executable_path);
    free(helper_path);
    return def;
  }
  free(helper_path);
  return value;
}
