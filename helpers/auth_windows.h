#ifndef XSECURELOCK_HELPERS_AUTH_WINDOWS_H
#define XSECURELOCK_HELPERS_AUTH_WINDOWS_H

#include <stddef.h>

#include "auth_ui.h"

int AuthWindowsUpdate(struct AuthUiContext *ctx, int region_w, int region_h);
void AuthWindowsDestroy(struct AuthUiContext *ctx, size_t keep_windows);

#endif  // XSECURELOCK_HELPERS_AUTH_WINDOWS_H
