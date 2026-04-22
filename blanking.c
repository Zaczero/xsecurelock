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

void LockMaybeBlankScreen(struct LockContext *ctx) {
  if (ctx->config.blank_timeout < 0 || ctx->blanking.blanked) {
    return;
  }

  int64_t now_ms = 0;
  if (GetMonotonicTimeMs(&now_ms) != 0) {
    return;
  }
  if (now_ms < ctx->blanking.time_to_blank_ms) {
    return;
  }

  ctx->blanking.blanked = true;
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
    CARD16 state = 0;
    BOOL onoff = False;
    DPMSInfo(ctx->runtime.display, &state, &onoff);
    if (!onoff) {
      ctx->blanking.must_disable_dpms = true;
      DPMSEnable(ctx->runtime.display);
    }
    if (!strcmp(ctx->config.blank_dpms_state, "standby")) {
      DPMSForceLevel(ctx->runtime.display, DPMSModeStandby);
    } else if (!strcmp(ctx->config.blank_dpms_state, "suspend")) {
      DPMSForceLevel(ctx->runtime.display, DPMSModeSuspend);
    } else if (!strcmp(ctx->config.blank_dpms_state, "off")) {
      DPMSForceLevel(ctx->runtime.display, DPMSModeOff);
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
