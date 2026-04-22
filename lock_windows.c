#include "config.h"

#include "lock_windows.h"

#include <X11/X.h>
#include <X11/Xatom.h>
#include <X11/Xcms.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef HAVE_XCOMPOSITE_EXT
#include <X11/extensions/Xcomposite.h>

#include "incompatible_compositor.xbm"
#endif
#ifdef HAVE_XFIXES_EXT
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/shapeconst.h>
#endif

#include "grabs.h"
#include "logging.h"
#include "wm_properties.h"

static int AddTrackedWindow(struct LockContext *ctx, Window window) {
  if (ctx->windows.tracked_window_count >= LOCK_TRACKED_WINDOW_CAPACITY) {
    Log("Too many tracked windows");
    return 0;
  }
  ctx->windows.tracked_windows[ctx->windows.tracked_window_count++] = window;
  return 1;
}

static int ValidWindowSize(int width, int height) {
  return width > 0 && height > 0;
}

static unsigned int ObscurerDimension(int size) {
  return (size > 2) ? (unsigned int)(size - 2) : 1U;
}

static int CreateTrackedCoverWindow(struct LockContext *ctx, Window *window,
                                    Window parent, int x, int y,
                                    unsigned int width, unsigned int height,
                                    unsigned long valuemask,
                                    XSetWindowAttributes *attrs,
                                    const char *role) {
  if (!ValidWindowSize((int)width, (int)height)) {
    Log("%s window has invalid size %ux%u", role, width, height);
    *window = None;
    return 0;
  }

  *window = XCreateWindow(ctx->runtime.display, parent, x, y, width, height, 0,
                          CopyFromParent, InputOutput, CopyFromParent,
                          valuemask, attrs);
  if (*window == None) {
    Log("XCreateWindow failed for %s", role);
    return 0;
  }
  SetWMProperties(ctx->runtime.display, *window, "xsecurelock", role,
                  ctx->config.argc, ctx->config.argv);
  if (!AddTrackedWindow(ctx, *window)) {
    XDestroyWindow(ctx->runtime.display, *window);
    *window = None;
    return 0;
  }
  return 1;
}

static void DestroyWindowIfAny(Display *display, Window *window) {
  if (*window != None) {
    XDestroyWindow(display, *window);
    *window = None;
  }
}

static void FreeCursorIfAny(Display *display, Cursor *cursor) {
  if (*cursor != None) {
    XFreeCursor(display, *cursor);
    *cursor = None;
  }
}

static void FreePixmapIfAny(Display *display, Pixmap *pixmap) {
  if (*pixmap != None) {
    XFreePixmap(display, *pixmap);
    *pixmap = None;
  }
}

static void SelectWindowInputs(struct LockContext *ctx) {
#ifdef HAVE_XCOMPOSITE_EXT
  if (ctx->windows.composite_window != None) {
    XSelectInput(ctx->runtime.display, ctx->windows.composite_window,
                 StructureNotifyMask | VisibilityChangeMask);
  }
  if (ctx->windows.obscurer_window != None) {
    XSelectInput(ctx->runtime.display, ctx->windows.obscurer_window,
                 StructureNotifyMask | VisibilityChangeMask);
  }
#endif
  XSelectInput(ctx->runtime.display, ctx->windows.background_window,
               StructureNotifyMask | VisibilityChangeMask);
  XSelectInput(ctx->runtime.display, ctx->windows.saver_window,
               StructureNotifyMask);
  XSelectInput(ctx->runtime.display, ctx->windows.auth_window,
               StructureNotifyMask | VisibilityChangeMask);
}

static void SetWindowProperties(struct LockContext *ctx) {
  XWindowChanges coverchanges = {.stack_mode = Above};
  XConfigureWindow(ctx->runtime.display, ctx->windows.background_window,
                   CWStackMode, &coverchanges);
  XConfigureWindow(ctx->runtime.display, ctx->windows.auth_window,
                   CWStackMode, &coverchanges);

  Atom state_atom = XInternAtom(ctx->runtime.display, "_NET_WM_STATE", False);
  Atom fullscreen_atom =
      XInternAtom(ctx->runtime.display, "_NET_WM_STATE_FULLSCREEN", False);
  XChangeProperty(ctx->runtime.display, ctx->windows.background_window,
                  state_atom, XA_ATOM, 32, PropModeReplace,
                  (const unsigned char *)&fullscreen_atom, 1);

  Atom dont_composite_atom =
      XInternAtom(ctx->runtime.display, "_NET_WM_BYPASS_COMPOSITOR", False);
  long dont_composite = 1;
  XChangeProperty(ctx->runtime.display, ctx->windows.background_window,
                  dont_composite_atom, XA_CARDINAL, 32, PropModeReplace,
                  (const unsigned char *)&dont_composite, 1);
#ifdef HAVE_XCOMPOSITE_EXT
  if (ctx->windows.composite_window != None) {
    XChangeProperty(ctx->runtime.display, ctx->windows.composite_window,
                    dont_composite_atom, XA_CARDINAL, 32, PropModeReplace,
                    (const unsigned char *)&dont_composite, 1);
  }
#endif
}

static void InitInputMethod(struct LockContext *ctx) {
  ctx->windows.xim = XOpenIM(ctx->runtime.display, NULL, NULL, NULL);
  if (ctx->windows.xim == NULL) {
    Log("XOpenIM failed. Assuming Latin-1 encoding");
    return;
  }

  static const int input_styles[] = {
      XIMPreeditNothing | XIMStatusNothing,
      XIMPreeditNothing | XIMStatusNone,
      XIMPreeditNone | XIMStatusNothing,
      XIMPreeditNone | XIMStatusNone,
  };
  for (size_t i = 0; i < sizeof(input_styles) / sizeof(input_styles[0]); ++i) {
    ctx->windows.xic =
        XCreateIC(ctx->windows.xim, XNInputStyle, input_styles[i],
                  XNClientWindow, ctx->windows.auth_window, NULL);
    if (ctx->windows.xic != NULL) {
      return;
    }
  }
  Log("XCreateIC failed. Assuming Latin-1 encoding");
}

int LockWindowsInit(struct LockContext *ctx) {
  ctx->windows.root_window = DefaultRootWindow(ctx->runtime.display);
  XSelectInput(ctx->runtime.display, ctx->windows.root_window,
               StructureNotifyMask | FocusChangeMask);

  ctx->windows.width =
      DisplayWidth(ctx->runtime.display, DefaultScreen(ctx->runtime.display));
  ctx->windows.height =
      DisplayHeight(ctx->runtime.display, DefaultScreen(ctx->runtime.display));

  XColor black = {.pixel =
                      BlackPixel(ctx->runtime.display,
                                 DefaultScreen(ctx->runtime.display))};
  XQueryColor(ctx->runtime.display,
              DefaultColormap(ctx->runtime.display,
                              DefaultScreen(ctx->runtime.display)),
              &black);

  unsigned long background_pixel = black.pixel;
  XColor xcolor_background;
  if (XAllocNamedColor(ctx->runtime.display,
                       DefaultColormap(ctx->runtime.display,
                                       DefaultScreen(ctx->runtime.display)),
                       ctx->config.background_color, &xcolor_background,
                       &(XColor){0}) != XcmsFailure) {
    background_pixel = xcolor_background.pixel;
  }

  ctx->windows.bg =
      XCreateBitmapFromData(ctx->runtime.display, ctx->windows.root_window,
                            "\0", 1, 1);
  if (ctx->windows.bg == None) {
    Log("XCreateBitmapFromData failed");
    return 0;
  }
  ctx->windows.default_cursor =
      XCreateFontCursor(ctx->runtime.display, XC_arrow);
  if (ctx->windows.default_cursor == None) {
    Log("XCreateFontCursor failed");
    return 0;
  }
  ctx->windows.transparent_cursor = XCreatePixmapCursor(
      ctx->runtime.display, ctx->windows.bg, ctx->windows.bg, &black, &black,
      0, 0);
  if (ctx->windows.transparent_cursor == None) {
    Log("XCreatePixmapCursor failed");
    return 0;
  }

  XSetWindowAttributes coverattrs = {
      .background_pixel = background_pixel,
      .save_under = 1,
      .override_redirect = 1,
      .cursor = ctx->windows.transparent_cursor,
  };
  ctx->windows.parent_window = ctx->windows.root_window;

#ifdef HAVE_XCOMPOSITE_EXT
  {
    int composite_event_base = 0;
    int composite_error_base = 0;
    int composite_major_version = 0;
    int composite_minor_version = 0;
    ctx->windows.have_xcomposite_ext =
        XCompositeQueryExtension(ctx->runtime.display, &composite_event_base,
                                 &composite_error_base) &&
        XCompositeQueryVersion(ctx->runtime.display, &composite_major_version,
                               &composite_minor_version) &&
        (composite_major_version >= 1 || composite_minor_version >= 3);
  }
  if (!ctx->windows.have_xcomposite_ext) {
    Log("XComposite extension not detected");
  }
  if (ctx->windows.have_xcomposite_ext && ctx->config.no_composite) {
    Log("XComposite extension detected but disabled by user");
    ctx->windows.have_xcomposite_ext = false;
  }
  if (ctx->windows.have_xcomposite_ext) {
    ctx->windows.composite_window = XCompositeGetOverlayWindow(
        ctx->runtime.display, ctx->windows.root_window);
    if (ctx->windows.composite_window == None) {
      Log("XCompositeGetOverlayWindow failed");
      ctx->windows.have_xcomposite_ext = false;
    }
  }
  if (ctx->windows.have_xcomposite_ext) {
    XMapRaised(ctx->runtime.display, ctx->windows.composite_window);
#ifdef HAVE_XFIXES_EXT
    {
      int xfixes_event_base = 0;
      int xfixes_error_base = 0;
      if (XFixesQueryExtension(ctx->runtime.display, &xfixes_event_base,
                               &xfixes_error_base)) {
        XFixesSetWindowShapeRegion(ctx->runtime.display,
                                   ctx->windows.composite_window,
                                   ShapeBounding, 0, 0, 0);
      }
    }
#endif
    ctx->windows.parent_window = ctx->windows.composite_window;

    if (ctx->config.composite_obscurer) {
      XSetWindowAttributes obscurerattrs = coverattrs;
      ctx->windows.obscurer_background_pixmap = XCreatePixmapFromBitmapData(
          ctx->runtime.display, ctx->windows.root_window,
          incompatible_compositor_bits, incompatible_compositor_width,
          incompatible_compositor_height,
          BlackPixel(ctx->runtime.display,
                     DefaultScreen(ctx->runtime.display)),
          WhitePixel(ctx->runtime.display,
                     DefaultScreen(ctx->runtime.display)),
          DefaultDepth(ctx->runtime.display,
                       DefaultScreen(ctx->runtime.display)));
      if (ctx->windows.obscurer_background_pixmap == None) {
        Log("XCreatePixmapFromBitmapData failed");
        return 0;
      }
      obscurerattrs.background_pixmap = ctx->windows.obscurer_background_pixmap;
      if (!CreateTrackedCoverWindow(
              ctx, &ctx->windows.obscurer_window, ctx->windows.root_window, 1,
              1, ObscurerDimension(ctx->windows.width),
              ObscurerDimension(ctx->windows.height),
              CWBackPixmap | CWSaveUnder | CWOverrideRedirect | CWCursor,
              &obscurerattrs, "obscurer")) {
        return 0;
      }
    }
  }
#endif

  if (!CreateTrackedCoverWindow(
          ctx, &ctx->windows.background_window, ctx->windows.parent_window, 0,
          0, (unsigned int)ctx->windows.width, (unsigned int)ctx->windows.height,
          CWBackPixel | CWSaveUnder | CWOverrideRedirect | CWCursor, &coverattrs,
          "background")) {
    return 0;
  }

  if (!CreateTrackedCoverWindow(
          ctx, &ctx->windows.saver_window, ctx->windows.background_window, 0, 0,
          (unsigned int)ctx->windows.width, (unsigned int)ctx->windows.height,
          CWBackPixel, &coverattrs, "saver")) {
    return 0;
  }

  if (!CreateTrackedCoverWindow(
          ctx, &ctx->windows.auth_window, ctx->windows.background_window, 0, 0,
          (unsigned int)ctx->windows.width, (unsigned int)ctx->windows.height,
          CWBackPixel, &coverattrs, "auth")) {
    return 0;
  }

  SelectWindowInputs(ctx);
  SetWindowProperties(ctx);
  InitInputMethod(ctx);
  return 1;
}

void LockWindowsMap(struct LockContext *ctx) {
  XMapRaised(ctx->runtime.display, ctx->windows.saver_window);
  XMapRaised(ctx->runtime.display, ctx->windows.background_window);
  XClearWindow(ctx->runtime.display, ctx->windows.background_window);
  XRaiseWindow(ctx->runtime.display, ctx->windows.auth_window);
#ifdef HAVE_XCOMPOSITE_EXT
  if (ctx->windows.obscurer_window != None) {
    XMapRaised(ctx->runtime.display, ctx->windows.obscurer_window);
  }
#endif
  XFlush(ctx->runtime.display);
}

void LockWindowsResizeToRoot(struct LockContext *ctx, int width, int height) {
  ctx->windows.width = width;
  ctx->windows.height = height;
#ifdef HAVE_XCOMPOSITE_EXT
  if (ctx->windows.obscurer_window != None) {
    XMoveResizeWindow(ctx->runtime.display, ctx->windows.obscurer_window, 1, 1,
                      ObscurerDimension(width), ObscurerDimension(height));
  }
#endif
  XMoveResizeWindow(ctx->runtime.display, ctx->windows.background_window, 0, 0,
                    width, height);
  XClearWindow(ctx->runtime.display, ctx->windows.background_window);
  XMoveResizeWindow(ctx->runtime.display, ctx->windows.saver_window, 0, 0, width,
                    height);
}

void LockWindowsHandleConfigureNotify(struct LockContext *ctx,
                                      const XConfigureEvent *ev) {
  if (ev->window == ctx->windows.root_window) {
    LockWindowsResizeToRoot(ctx, ev->width, ev->height);
  }
  if (ctx->windows.auth_window_mapped && ev->window == ctx->windows.auth_window) {
    LockMaybeRaiseWindow(ctx, ctx->windows.auth_window, 0, 0);
  } else if (ev->window == ctx->windows.background_window) {
    LockMaybeRaiseWindow(ctx, ctx->windows.background_window, 0, 0);
    XClearWindow(ctx->runtime.display, ctx->windows.background_window);
#ifdef HAVE_XCOMPOSITE_EXT
  } else if (ctx->windows.obscurer_window != None &&
             ev->window == ctx->windows.obscurer_window) {
    LockMaybeRaiseWindow(ctx, ctx->windows.obscurer_window, 1, 0);
#endif
  }
}

void LockWindowsHandleVisibilityNotify(struct LockContext *ctx,
                                       const XVisibilityEvent *ev) {
  if (ev->state == VisibilityUnobscured) {
    if (ev->window == ctx->windows.background_window) {
      ctx->windows.background_window_visible = true;
    }
    return;
  }

  if (ctx->windows.auth_window_mapped && ev->window == ctx->windows.auth_window) {
    Log("Someone overlapped the auth window. Undoing that");
    LockMaybeRaiseWindow(ctx, ctx->windows.auth_window, 0, 1);
  } else if (ev->window == ctx->windows.background_window) {
    ctx->windows.background_window_visible = false;
    Log("Someone overlapped the background window. Undoing that");
    LockMaybeRaiseWindow(ctx, ctx->windows.background_window, 0, 1);
    XClearWindow(ctx->runtime.display, ctx->windows.background_window);
#ifdef HAVE_XCOMPOSITE_EXT
  } else if (ctx->windows.obscurer_window != None &&
             ev->window == ctx->windows.obscurer_window) {
    LockMaybeRaiseWindow(ctx, ctx->windows.obscurer_window, 1, 1);
  } else if (ctx->windows.composite_window != None &&
             ev->window == ctx->windows.composite_window) {
    Log("Someone overlapped the composite overlay window window. Undoing that");
    XRaiseWindow(ctx->runtime.display, ctx->windows.composite_window);
#endif
  } else {
    Log("Received unexpected VisibilityNotify for window %lu", ev->window);
  }
}

void LockWindowsHandleMapNotify(struct LockContext *ctx, const XMapEvent *ev) {
  if (ev->window == ctx->windows.auth_window) {
    ctx->windows.auth_window_mapped = true;
#ifdef SHOW_CURSOR_DURING_AUTH
    XGrabPointer(ctx->runtime.display, ctx->windows.root_window, False,
                 ALL_POINTER_EVENTS, GrabModeAsync, GrabModeAsync, None,
                 ctx->windows.default_cursor, CurrentTime);
#endif
  } else if (ev->window == ctx->windows.saver_window) {
    ctx->windows.saver_window_mapped = true;
  } else if (ev->window == ctx->windows.background_window) {
    ctx->windows.background_window_mapped = true;
  }
}

void LockWindowsHandleUnmapNotify(struct LockContext *ctx,
                                  const XUnmapEvent *ev) {
  if (ev->window == ctx->windows.auth_window) {
    ctx->windows.auth_window_mapped = false;
#ifdef SHOW_CURSOR_DURING_AUTH
    XGrabPointer(ctx->runtime.display, ctx->windows.root_window, False,
                 ALL_POINTER_EVENTS, GrabModeAsync, GrabModeAsync, None,
                 ctx->windows.transparent_cursor, CurrentTime);
#endif
  } else if (ev->window == ctx->windows.saver_window) {
    Log("Someone unmapped the saver window. Undoing that");
    ctx->windows.saver_window_mapped = false;
    XMapWindow(ctx->runtime.display, ctx->windows.saver_window);
  } else if (ev->window == ctx->windows.background_window) {
    Log("Someone unmapped the background window. Undoing that");
    ctx->windows.background_window_mapped = false;
    XMapRaised(ctx->runtime.display, ctx->windows.background_window);
    XClearWindow(ctx->runtime.display, ctx->windows.background_window);
#ifdef HAVE_XCOMPOSITE_EXT
  } else if (ctx->windows.obscurer_window != None &&
             ev->window == ctx->windows.obscurer_window) {
    Log("Someone unmapped the obscurer window. Undoing that");
    XMapRaised(ctx->runtime.display, ctx->windows.obscurer_window);
  } else if (ctx->windows.composite_window != None &&
             ev->window == ctx->windows.composite_window) {
    Log("Someone unmapped the composite overlay window. Undoing that");
    XMapRaised(ctx->runtime.display, ctx->windows.composite_window);
#endif
  } else if (ev->window == ctx->windows.root_window) {
    Log("Someone unmapped the root window?!? Undoing that");
    XMapRaised(ctx->runtime.display, ctx->windows.root_window);
  }
}

int LockWindowsReadyToNotify(const struct LockContext *ctx) {
  return ctx->windows.background_window_mapped &&
         ctx->windows.background_window_visible &&
         ctx->windows.saver_window_mapped;
}

void LockWindowsMarkNotified(struct LockContext *ctx) {
  ctx->windows.xss_lock_notified = true;
}

void LockWindowsCleanup(struct LockContext *ctx) {
  if (ctx->runtime.display == NULL) {
    return;
  }

  if (ctx->windows.xic != NULL) {
    XDestroyIC(ctx->windows.xic);
    ctx->windows.xic = NULL;
  }
  if (ctx->windows.xim != NULL) {
    XCloseIM(ctx->windows.xim);
    ctx->windows.xim = NULL;
  }

#ifdef HAVE_XCOMPOSITE_EXT
  DestroyWindowIfAny(ctx->runtime.display, &ctx->windows.obscurer_window);
  FreePixmapIfAny(ctx->runtime.display, &ctx->windows.obscurer_background_pixmap);
#endif
  DestroyWindowIfAny(ctx->runtime.display, &ctx->windows.auth_window);
  DestroyWindowIfAny(ctx->runtime.display, &ctx->windows.saver_window);
  DestroyWindowIfAny(ctx->runtime.display, &ctx->windows.background_window);
#ifdef HAVE_XCOMPOSITE_EXT
  if (ctx->windows.composite_window != None) {
    XCompositeReleaseOverlayWindow(ctx->runtime.display,
                                   ctx->windows.composite_window);
    ctx->windows.composite_window = None;
  }
#endif

  FreeCursorIfAny(ctx->runtime.display, &ctx->windows.transparent_cursor);
  FreeCursorIfAny(ctx->runtime.display, &ctx->windows.default_cursor);
  FreePixmapIfAny(ctx->runtime.display, &ctx->windows.bg);
}
