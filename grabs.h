#ifndef XSECURELOCK_GRABS_H
#define XSECURELOCK_GRABS_H

#include "lock_state.h"

int LockAcquireGrabs(struct LockContext *ctx, int silent, int force);
void LockMaybeRaiseWindow(struct LockContext *ctx, Window w, int silent,
                          int force);

#endif  // XSECURELOCK_GRABS_H
