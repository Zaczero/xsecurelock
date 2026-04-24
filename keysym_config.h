// SPDX-License-Identifier: Apache-2.0

#ifndef XSECURELOCK_KEYSYM_CONFIG_H
#define XSECURELOCK_KEYSYM_CONFIG_H

#include <X11/Xlib.h>

const char *GetKeySymSetting(const char *name, const char *def, KeySym *keysym);

#endif  // XSECURELOCK_KEYSYM_CONFIG_H
