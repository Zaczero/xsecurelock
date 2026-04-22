#ifndef XSECURELOCK_LOCK_WINDOWS_H
#define XSECURELOCK_LOCK_WINDOWS_H

#include "lock_state.h"

int LockWindowsInit(struct LockContext *ctx);
void LockWindowsMap(struct LockContext *ctx);
void LockWindowsResizeToRoot(struct LockContext *ctx, int width, int height);
void LockWindowsHandleMapNotify(struct LockContext *ctx, const XMapEvent *ev);
void LockWindowsHandleUnmapNotify(struct LockContext *ctx,
                                  const XUnmapEvent *ev);
void LockWindowsHandleVisibilityNotify(struct LockContext *ctx,
                                       const XVisibilityEvent *ev);
void LockWindowsHandleConfigureNotify(struct LockContext *ctx,
                                      const XConfigureEvent *ev);
int LockWindowsReadyToNotify(const struct LockContext *ctx);
void LockWindowsMarkNotified(struct LockContext *ctx);
void LockWindowsCleanup(struct LockContext *ctx);

#endif  // XSECURELOCK_LOCK_WINDOWS_H
