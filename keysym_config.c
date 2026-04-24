// SPDX-License-Identifier: Apache-2.0

#include "config.h"

#include "keysym_config.h"

#include "env_settings.h"
#include "logging.h"

const char *GetKeySymSetting(const char *name, const char *def,
                             KeySym *keysym) {
  const char *value = GetStringSetting(name, def);
  KeySym parsed = NoSymbol;
  if (value[0] != '\0') {
    parsed = XStringToKeysym(value);
  }

  if (parsed == NoSymbol) {
    Log("Ignoring invalid keysym value of %s: %s", name, value);
    parsed = XStringToKeysym(def);
  }

  const char *canonical = XKeysymToString(parsed);
  if (canonical == NULL) {
    Log("Invalid default keysym value for %s: %s", name, def);
    canonical = def;
  }

  if (keysym != NULL) {
    *keysym = parsed;
  }
  return canonical;
}
