#ifndef XSECURELOCK_BLANKING_H
#define XSECURELOCK_BLANKING_H

#include "lock_state.h"

void LockBlankingResetTimer(struct LockContext *ctx);
void LockBlankingInit(struct LockContext *ctx);
void LockMaybeBlankScreen(struct LockContext *ctx);
void LockScreenNoLongerBlanked(struct LockContext *ctx);
void LockUnblankScreen(struct LockContext *ctx);

#endif  // XSECURELOCK_BLANKING_H
