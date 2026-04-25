#include "config.h"

#include "blanking.h"

#include <X11/Xlib.h>
#include <string.h>

#ifdef HAVE_DPMS_EXT
#include <X11/Xmd.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsconst.h>
#endif

#include "logging.h"
#include "time_util.h"

#define DPMS_REAPPLY_INTERVAL_MS 1000

void LockBlankingResetTimer(struct LockContext *ctx) {
  if (ctx->config.blank_timeout < 0) {
    return;
  }
  if (GetMonotonicTimeMs(&ctx->blanking.time_to_blank_ms) != 0) {
    ctx->blanking.time_to_blank_ms = INT64_MAX;
    return;
  }
  ctx->blanking.time_to_blank_ms += (int64_t)ctx->config.blank_timeout * 1000;
}

void LockBlankingInit(struct LockContext *ctx) {
  if (ctx->config.blank_timeout < 0) {
    return;
  }
  ctx->blanking.blanked = false;
  LockBlankingResetTimer(ctx);
}

#ifdef HAVE_DPMS_EXT
static int GetBlankingDpmsMode(const char *blank_dpms_state, CARD16 *mode) {
  if (!strcmp(blank_dpms_state, "standby")) {
    *mode = DPMSModeStandby;
  } else if (!strcmp(blank_dpms_state, "suspend")) {
    *mode = DPMSModeSuspend;
  } else if (!strcmp(blank_dpms_state, "off")) {
    *mode = DPMSModeOff;
  } else {
    return 0;
  }
  return 1;
}

static void ForceBlankingDpmsMode(struct LockContext *ctx, CARD16 mode,
                                  BOOL onoff, int set_disable_on_unblank) {
  if (!onoff) {
    if (set_disable_on_unblank) {
      ctx->blanking.must_disable_dpms = true;
    }
    DPMSEnable(ctx->runtime.display);
  }
  DPMSForceLevel(ctx->runtime.display, mode);
}

static void LockMaybeReapplyDpms(struct LockContext *ctx, int64_t now_ms) {
  if (!strcmp(ctx->config.blank_dpms_state, "on")) {
    return;
  }

  CARD16 mode = 0;
  if (!GetBlankingDpmsMode(ctx->config.blank_dpms_state, &mode)) {
    return;
  }
  if (now_ms < ctx->blanking.next_dpms_reapply_ms) {
    return;
  }
  ctx->blanking.next_dpms_reapply_ms = now_ms + DPMS_REAPPLY_INTERVAL_MS;

  int dummy = 0;
  if (!DPMSQueryExtension(ctx->runtime.display, &dummy, &dummy)) {
    return;
  }

  CARD16 state = 0;
  BOOL onoff = False;
  DPMSInfo(ctx->runtime.display, &state, &onoff);
  if (!onoff || state != mode) {
    ForceBlankingDpmsMode(ctx, mode, onoff, 1);
    XFlush(ctx->runtime.display);
  }
}
#endif

void LockMaybeBlankScreen(struct LockContext *ctx) {
  if (ctx->config.blank_timeout < 0) {
    return;
  }

  int64_t now_ms = 0;
  if (GetMonotonicTimeMs(&now_ms) != 0) {
    return;
  }
  if (ctx->blanking.blanked) {
#ifdef HAVE_DPMS_EXT
    LockMaybeReapplyDpms(ctx, now_ms);
#endif
    return;
  }
  if (now_ms < ctx->blanking.time_to_blank_ms) {
    return;
  }

  ctx->blanking.blanked = true;
#ifdef HAVE_DPMS_EXT
  ctx->blanking.next_dpms_reapply_ms = now_ms + DPMS_REAPPLY_INTERVAL_MS;
#endif
  XForceScreenSaver(ctx->runtime.display, ScreenSaverActive);
  if (!strcmp(ctx->config.blank_dpms_state, "on")) {
    goto done;
  }

#ifdef HAVE_DPMS_EXT
  {
    int dummy = 0;
    if (!DPMSQueryExtension(ctx->runtime.display, &dummy, &dummy)) {
      Log("DPMS is unavailable and XSECURELOCK_BLANK_DPMS_STATE not on");
      goto done;
    }
  }

  {
    CARD16 mode = 0;
    if (GetBlankingDpmsMode(ctx->config.blank_dpms_state, &mode)) {
      CARD16 state = 0;
      BOOL onoff = False;
      DPMSInfo(ctx->runtime.display, &state, &onoff);
      ForceBlankingDpmsMode(ctx, mode, onoff, 1);
    } else {
      Log("XSECURELOCK_BLANK_DPMS_STATE not in standby/suspend/off/on");
    }
  }
#else
  Log("DPMS is not compiled in and XSECURELOCK_BLANK_DPMS_STATE not on");
#endif

done:
  XFlush(ctx->runtime.display);
}

void LockScreenNoLongerBlanked(struct LockContext *ctx) {
#ifdef HAVE_DPMS_EXT
  if (ctx->blanking.must_disable_dpms) {
    DPMSDisable(ctx->runtime.display);
    ctx->blanking.must_disable_dpms = false;
    XFlush(ctx->runtime.display);
  }
#endif
  ctx->blanking.blanked = false;
}

void LockUnblankScreen(struct LockContext *ctx) {
  if (ctx->blanking.blanked) {
    XForceScreenSaver(ctx->runtime.display, ScreenSaverReset);
    LockScreenNoLongerBlanked(ctx);
  }
  LockBlankingResetTimer(ctx);
}
