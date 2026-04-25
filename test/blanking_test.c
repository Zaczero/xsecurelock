#include "config.h"

#include <X11/Xlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

#ifdef HAVE_DPMS_EXT
#include <X11/Xmd.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/dpmsconst.h>
#endif

#include "logging.h"
#include "time_util.h"

static int64_t now_ms;
static int x_force_screen_saver_count;
static int x_force_screen_saver_mode;
static int x_flush_count;

#ifdef HAVE_DPMS_EXT
static Bool dpms_extension_available;
static BOOL dpms_enabled;
static CARD16 dpms_state;
static int dpms_query_count;
static int dpms_info_count;
static int dpms_enable_count;
static int dpms_disable_count;
static int dpms_force_count;
static CARD16 dpms_forced_level;
#endif

int GetMonotonicTimeMs(int64_t *time_ms) {
  *time_ms = now_ms;
  return 0;
}

void Log(const char *format, ...) { (void)format; }

int XForceScreenSaver(Display *display, int mode) {
  (void)display;
  ++x_force_screen_saver_count;
  x_force_screen_saver_mode = mode;
  return 0;
}

int XFlush(Display *display) {
  (void)display;
  ++x_flush_count;
  return 0;
}

#ifdef HAVE_DPMS_EXT
Bool DPMSQueryExtension(Display *display, int *event_base, int *error_base) {
  (void)display;
  *event_base = 0;
  *error_base = 0;
  ++dpms_query_count;
  return dpms_extension_available;
}

Status DPMSInfo(Display *display, CARD16 *power_level, BOOL *state) {
  (void)display;
  *power_level = dpms_state;
  *state = dpms_enabled;
  ++dpms_info_count;
  return 1;
}

Status DPMSEnable(Display *display) {
  (void)display;
  dpms_enabled = True;
  ++dpms_enable_count;
  return 1;
}

Status DPMSDisable(Display *display) {
  (void)display;
  dpms_enabled = False;
  ++dpms_disable_count;
  return 1;
}

Status DPMSForceLevel(Display *display, CARD16 level) {
  (void)display;
  dpms_state = level;
  ++dpms_force_count;
  dpms_forced_level = level;
  return 1;
}
#endif

#include "../blanking.c"

static void ResetFakes(void) {
  now_ms = 0;
  x_force_screen_saver_count = 0;
  x_force_screen_saver_mode = -1;
  x_flush_count = 0;
#ifdef HAVE_DPMS_EXT
  dpms_extension_available = True;
  dpms_enabled = True;
  dpms_state = DPMSModeOn;
  dpms_query_count = 0;
  dpms_info_count = 0;
  dpms_enable_count = 0;
  dpms_disable_count = 0;
  dpms_force_count = 0;
  dpms_forced_level = 0;
#endif
}

static struct LockContext BlankContext(const char *dpms_state_name) {
  struct LockContext ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.config.blank_timeout = 0;
  ctx.config.blank_dpms_state = dpms_state_name;
  return ctx;
}

static void TestInitialBlankForcesConfiguredDpms(void) {
  ResetFakes();
  struct LockContext ctx = BlankContext("off");
  LockBlankingInit(&ctx);
  LockMaybeBlankScreen(&ctx);

  assert(ctx.blanking.blanked);
  assert(x_force_screen_saver_count == 1);
  assert(x_force_screen_saver_mode == ScreenSaverActive);
  assert(x_flush_count == 1);
#ifdef HAVE_DPMS_EXT
  assert(dpms_force_count == 1);
  assert(dpms_forced_level == DPMSModeOff);
  assert(!ctx.blanking.must_disable_dpms);
#endif
}

static void TestDisabledDpmsIsRestoredOnUnblank(void) {
  ResetFakes();
  struct LockContext ctx = BlankContext("standby");
#ifdef HAVE_DPMS_EXT
  dpms_enabled = False;
#endif
  LockBlankingInit(&ctx);
  LockMaybeBlankScreen(&ctx);

#ifdef HAVE_DPMS_EXT
  assert(dpms_enable_count == 1);
  assert(dpms_forced_level == DPMSModeStandby);
  assert(ctx.blanking.must_disable_dpms);
#endif
  LockUnblankScreen(&ctx);
  assert(!ctx.blanking.blanked);
  assert(x_force_screen_saver_mode == ScreenSaverReset);
#ifdef HAVE_DPMS_EXT
  assert(dpms_disable_count == 1);
  assert(!ctx.blanking.must_disable_dpms);
#endif
}

static void TestNoDpmsCallsWhenConfiguredOn(void) {
  ResetFakes();
  struct LockContext ctx = BlankContext("on");
  LockBlankingInit(&ctx);
  LockMaybeBlankScreen(&ctx);

  assert(ctx.blanking.blanked);
  assert(x_force_screen_saver_count == 1);
#ifdef HAVE_DPMS_EXT
  assert(dpms_query_count == 0);
  assert(dpms_info_count == 0);
  assert(dpms_force_count == 0);
#endif
}

static void TestDpmsWakeIsReappliedAfterInterval(void) {
  ResetFakes();
  struct LockContext ctx = BlankContext("suspend");
  LockBlankingInit(&ctx);
  LockMaybeBlankScreen(&ctx);

#ifdef HAVE_DPMS_EXT
  assert(dpms_force_count == 1);
  assert(dpms_forced_level == DPMSModeSuspend);
  assert(dpms_query_count == 1);

  dpms_state = DPMSModeOn;
  now_ms = DPMS_REAPPLY_INTERVAL_MS - 1;
  LockMaybeBlankScreen(&ctx);
  assert(dpms_force_count == 1);
  assert(dpms_query_count == 1);

  now_ms = DPMS_REAPPLY_INTERVAL_MS;
  LockMaybeBlankScreen(&ctx);
  assert(dpms_query_count == 2);
  assert(dpms_force_count == 2);
  assert(dpms_forced_level == DPMSModeSuspend);
#endif
}

static void TestDisabledDpmsDuringBlankIsRestoredOnUnblank(void) {
  ResetFakes();
  struct LockContext ctx = BlankContext("off");
  LockBlankingInit(&ctx);
  LockMaybeBlankScreen(&ctx);

#ifdef HAVE_DPMS_EXT
  assert(!ctx.blanking.must_disable_dpms);
  dpms_enabled = False;
  now_ms = DPMS_REAPPLY_INTERVAL_MS;
  LockMaybeBlankScreen(&ctx);
  assert(dpms_enable_count == 1);
  assert(ctx.blanking.must_disable_dpms);
#endif

  LockUnblankScreen(&ctx);
#ifdef HAVE_DPMS_EXT
  assert(dpms_disable_count == 1);
  assert(!ctx.blanking.must_disable_dpms);
#endif
}

static void TestDpmsAlreadyBlankedIsNotReapplied(void) {
  ResetFakes();
  struct LockContext ctx = BlankContext("off");
  LockBlankingInit(&ctx);
  LockMaybeBlankScreen(&ctx);

#ifdef HAVE_DPMS_EXT
  assert(dpms_force_count == 1);
  now_ms = DPMS_REAPPLY_INTERVAL_MS;
  LockMaybeBlankScreen(&ctx);
  assert(dpms_force_count == 1);
#endif
}

int main(void) {
  TestInitialBlankForcesConfiguredDpms();
  TestDisabledDpmsIsRestoredOnUnblank();
  TestNoDpmsCallsWhenConfiguredOn();
  TestDpmsWakeIsReappliedAfterInterval();
  TestDisabledDpmsDuringBlankIsRestoredOnUnblank();
  TestDpmsAlreadyBlankedIsNotReapplied();
  return 0;
}
