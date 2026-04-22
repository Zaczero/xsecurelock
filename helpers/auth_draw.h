#ifndef XSECURELOCK_HELPERS_AUTH_DRAW_H
#define XSECURELOCK_HELPERS_AUTH_DRAW_H

#include <stdbool.h>

#include "auth_ui.h"

void AuthPlaySound(struct AuthUiContext *ctx, enum AuthSound sound);
int AuthDisplayMessage(struct AuthUiContext *ctx, const char *title,
                       const char *message, bool warning);

#endif  // XSECURELOCK_HELPERS_AUTH_DRAW_H
