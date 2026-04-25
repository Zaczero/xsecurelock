#include "config.h"

#include "auth_windows.h"

#include <X11/X.h>
#include <X11/Xlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef HAVE_XFT_EXT
#include <X11/Xft/Xft.h>
#endif

#include "../logging.h"
#include "../wm_properties.h"

static void CleanupPerMonitorWindow(struct AuthUiContext *ctx, size_t i) {
  Display *display = ctx->resources.display;

  if (display == NULL || ctx->windows.windows[i] == None) {
    return;
  }

  XClearWindow(display, ctx->windows.windows[i]);
#ifdef HAVE_XFT_EXT
  if (ctx->windows.xft_draws[i] != NULL) {
    XftDrawDestroy(ctx->windows.xft_draws[i]);
    ctx->windows.xft_draws[i] = NULL;
  }
#endif
  if (ctx->windows.warning_gcs[i] != NULL) {
    XFreeGC(display, ctx->windows.warning_gcs[i]);
    ctx->windows.warning_gcs[i] = NULL;
  }
  if (ctx->windows.gcs[i] != NULL) {
    XFreeGC(display, ctx->windows.gcs[i]);
    ctx->windows.gcs[i] = NULL;
  }
  if (i == AUTH_UI_MAIN_WINDOW) {
    XUnmapWindow(display, ctx->windows.windows[i]);
  } else {
    XDestroyWindow(display, ctx->windows.windows[i]);
  }
  ctx->windows.windows[i] = None;
}

void AuthWindowsDestroy(struct AuthUiContext *ctx, size_t keep_windows) {
  for (size_t i = keep_windows; i < ctx->windows.count; ++i) {
    CleanupPerMonitorWindow(ctx, i);
  }
  if (ctx->windows.count > keep_windows) {
    ctx->windows.count = keep_windows;
  }
}

static void ClearWindowUncoveredAreas(struct AuthUiContext *ctx, size_t i,
                                      Rect new_rect) {
  Rect uncovered[4];
  for (size_t j = 0,
              count = RectSubtract(ctx->windows.rects[i], new_rect, uncovered);
       j < count; ++j) {
    XClearArea(ctx->resources.display, ctx->windows.windows[i],
               uncovered[j].x - ctx->windows.rects[i].x,
               uncovered[j].y - ctx->windows.rects[i].y, uncovered[j].w,
               uncovered[j].h, False);
  }
}

static int CreateOrUpdatePerMonitorWindow(struct AuthUiContext *ctx, size_t i,
                                          const Monitor *monitor, int region_w,
                                          int region_h) {
  int w = region_w;
  int h = region_h;
  int x;
  int y;
  int64_t desired_x =
      (int64_t)monitor->x +
      ((int64_t)monitor->width - (int64_t)w) * ctx->config.auth_x_position /
          100 +
      ctx->runtime.x_offset;
  int64_t desired_y =
      (int64_t)monitor->y +
      ((int64_t)monitor->height - (int64_t)h) * ctx->config.auth_y_position /
          100 +
      ctx->runtime.y_offset;
  if (desired_x < INT_MIN || desired_x > INT_MAX || desired_y < INT_MIN ||
      desired_y > INT_MAX) {
    Log("Auth dialog position is outside supported geometry range");
    return 0;
  }
  x = (int)desired_x;
  y = (int)desired_y;

  Rect new_rect;
  if (!RectClip((Rect){.x = x, .y = y, .w = w, .h = h},
                (Rect){.x = monitor->x,
                       .y = monitor->y,
                       .w = monitor->width,
                       .h = monitor->height},
                &new_rect)) {
    Log("Auth dialog has no visible area after clipping");
    return 0;
  }
  x = new_rect.x;
  y = new_rect.y;
  w = new_rect.w;
  h = new_rect.h;

  if (i < ctx->windows.count) {
    ClearWindowUncoveredAreas(ctx, i, new_rect);
    XMoveResizeWindow(ctx->resources.display, ctx->windows.windows[i], x, y, w,
                      h);
    ctx->windows.rects[i] = new_rect;
    return 1;
  }

  if (i > ctx->windows.count) {
    Log("Unreachable code - can't create monitor sequences with holes");
    abort();
  }

  {
    XSetWindowAttributes attrs = {0};
    attrs.background_pixel = ctx->resources.xcolor_background.pixel;
    if (i == AUTH_UI_MAIN_WINDOW) {
      XMoveResizeWindow(ctx->resources.display, ctx->resources.main_window, x,
                        y, w, h);
      XChangeWindowAttributes(ctx->resources.display,
                              ctx->resources.main_window, CWBackPixel, &attrs);
      ctx->windows.windows[i] = ctx->resources.main_window;
    } else {
      ctx->windows.windows[i] = XCreateWindow(
          ctx->resources.display, ctx->resources.parent_window, x, y, w, h, 0,
          CopyFromParent, InputOutput, CopyFromParent, CWBackPixel, &attrs);
      if (ctx->windows.windows[i] == None) {
        Log("XCreateWindow failed");
        return 0;
      }
      SetWMProperties(ctx->resources.display, ctx->windows.windows[i],
                      "xsecurelock", "auth_x11_screen", ctx->config.argc,
                      ctx->config.argv);
      Window stacking_order[2] = {ctx->resources.main_window,
                                  ctx->windows.windows[i]};
      XRestackWindows(ctx->resources.display, stacking_order, 2);
    }
  }

  {
    XGCValues gcattrs = {
        .function = GXcopy,
        .foreground = ctx->resources.xcolor_foreground.pixel,
        .background = ctx->resources.xcolor_background.pixel,
    };
    if (ctx->resources.core_font != NULL) {
      gcattrs.font = ctx->resources.core_font->fid;
    }
    ctx->windows.gcs[i] =
        XCreateGC(ctx->resources.display, ctx->windows.windows[i],
                  GCFunction | GCForeground | GCBackground |
                      (ctx->resources.core_font != NULL ? GCFont : 0),
                  &gcattrs);
    if (ctx->windows.gcs[i] == NULL) {
      Log("XCreateGC failed");
      CleanupPerMonitorWindow(ctx, i);
      return 0;
    }
    gcattrs.foreground = ctx->resources.xcolor_warning.pixel;
    ctx->windows.warning_gcs[i] =
        XCreateGC(ctx->resources.display, ctx->windows.windows[i],
                  GCFunction | GCForeground | GCBackground |
                      (ctx->resources.core_font != NULL ? GCFont : 0),
                  &gcattrs);
    if (ctx->windows.warning_gcs[i] == NULL) {
      Log("XCreateGC failed for warning GC");
      CleanupPerMonitorWindow(ctx, i);
      return 0;
    }
  }

#ifdef HAVE_XFT_EXT
  ctx->windows.xft_draws[i] = NULL;
  if (ctx->resources.xft_font != NULL) {
    ctx->windows.xft_draws[i] =
        XftDrawCreate(ctx->resources.display, ctx->windows.windows[i],
                      DefaultVisual(ctx->resources.display,
                                    DefaultScreen(ctx->resources.display)),
                      ctx->resources.colormap);
    if (ctx->windows.xft_draws[i] == NULL) {
      Log("XftDrawCreate failed");
      CleanupPerMonitorWindow(ctx, i);
      return 0;
    }
  }
#endif

  XMapWindow(ctx->resources.display, ctx->windows.windows[i]);
  ctx->windows.rects[i] = new_rect;
  ctx->windows.count = i + 1;
  return 1;
}

int AuthWindowsUpdate(struct AuthUiContext *ctx, int region_w, int region_h) {
  if (ctx->windows.dirty) {
    ctx->windows.monitor_count =
        GetMonitors(ctx->resources.display, ctx->resources.parent_window,
                    ctx->windows.monitors, AUTH_UI_MAX_WINDOWS);
  }

  if (ctx->config.single_auth_window) {
    Window unused_root = None;
    Window unused_child = None;
    int unused_root_x = 0;
    int unused_root_y = 0;
    int x = 0;
    int y = 0;
    unsigned int unused_mask = 0;

    XQueryPointer(ctx->resources.display, ctx->resources.parent_window,
                  &unused_root, &unused_child, &unused_root_x, &unused_root_y,
                  &x, &y, &unused_mask);
    for (size_t i = 0; i < ctx->windows.monitor_count; ++i) {
      if (RectContainsPoint((Rect){.x = ctx->windows.monitors[i].x,
                                   .y = ctx->windows.monitors[i].y,
                                   .w = ctx->windows.monitors[i].width,
                                   .h = ctx->windows.monitors[i].height},
                            x, y)) {
        if (!CreateOrUpdatePerMonitorWindow(ctx, 0, &ctx->windows.monitors[i],
                                            region_w, region_h)) {
          AuthWindowsDestroy(ctx, 0);
          return 0;
        }
        return 1;
      }
    }
    if (ctx->windows.monitor_count > 0) {
      if (!CreateOrUpdatePerMonitorWindow(ctx, 0, &ctx->windows.monitors[0],
                                          region_w, region_h)) {
        AuthWindowsDestroy(ctx, 0);
        return 0;
      }
      AuthWindowsDestroy(ctx, 1);
    } else {
      AuthWindowsDestroy(ctx, 0);
    }
    return 1;
  }

  for (size_t i = 0; i < ctx->windows.monitor_count; ++i) {
    if (!CreateOrUpdatePerMonitorWindow(ctx, i, &ctx->windows.monitors[i],
                                        region_w, region_h)) {
      AuthWindowsDestroy(ctx, 0);
      return 0;
    }
  }

  AuthWindowsDestroy(ctx, ctx->windows.monitor_count);

  if (ctx->windows.count != ctx->windows.monitor_count) {
    Log("Unreachable code - expected to get %d windows, got %d",
        (int)ctx->windows.monitor_count, (int)ctx->windows.count);
  }
  return 1;
}
