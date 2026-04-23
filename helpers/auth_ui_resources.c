#include "config.h"

#include "auth_ui_resources.h"

#include <X11/X.h>
#include <X11/Xlib.h>

#ifdef HAVE_XFT_EXT
#include <X11/Xft/Xft.h>
#include <X11/extensions/Xrender.h>
#include <fontconfig/fontconfig.h>
#endif

#include "../logging.h"
#include "../xscreensaver_api.h"
#include "xkb.h"

static int AllocNamedColorOrLog(struct AuthUiResources *resources,
                                const char *label, const char *color_name,
                                XColor *color, bool *allocated) {
  if (XAllocNamedColor(resources->display, resources->colormap, color_name,
                       color, &(XColor){0})) {
    *allocated = true;
    return 1;
  }
  Log("Could not allocate %s color '%s'", label, color_name);
  return 0;
}

#ifdef HAVE_XFT_EXT
static int AllocXftColorOrLog(struct AuthUiResources *resources,
                              const char *label, const XColor *xcolor,
                              XftColor *xft_color, bool *allocated) {
  XRenderColor xrcolor = {
      .red = xcolor->red,
      .green = xcolor->green,
      .blue = xcolor->blue,
      .alpha = 65535,
  };
  if (!XftColorAllocValue(
          resources->display,
          DefaultVisual(resources->display, DefaultScreen(resources->display)),
          resources->colormap, &xrcolor, xft_color)) {
    Log("XftColorAllocValue failed for %s color", label);
    return 0;
  }
  *allocated = true;
  return 1;
}

static XftFont *FixedXftFontOpenName(Display *display, int screen,
                                     const char *font_name) {
  XftFont *xft_font = XftFontOpenName(display, screen, font_name);
#if defined(HAVE_FONTCONFIG) && defined(FC_COLOR)
  FcBool iscol = FcFalse;
  if (xft_font != NULL &&
      FcPatternGetBool(xft_font->pattern, FC_COLOR, 0, &iscol) ==
          FcResultMatch &&
      iscol) {
    Log("Colored font %s is not supported by Xft", font_name);
    XftFontClose(display, xft_font);
    return NULL;
  }
#elif defined(HAVE_FONTCONFIG)
#warning "Fontconfig lacks FC_COLOR. May crash trying to use emoji fonts."
  Log("Fontconfig lacks FC_COLOR. May crash trying to use emoji fonts.");
#else
#warning "Xft enabled without fontconfig. May crash trying to use emoji fonts."
  Log("Xft enabled without fontconfig. May crash trying to use emoji fonts.");
#endif
  return xft_font;
}
#endif

int AuthUiResourcesInit(struct AuthUiResources *resources,
                        const struct AuthUiConfig *config) {
  int have_font = 0;
  Window unused_root = None;
  Window *unused_children = NULL;
  unsigned int unused_nchildren = 0;

  resources->display = XOpenDisplay(NULL);
  if (resources->display == NULL) {
    Log("Could not connect to $DISPLAY");
    goto fail;
  }

  int screen = DefaultScreen(resources->display);
  resources->colormap = DefaultColormap(resources->display, screen);
  resources->have_xkb_ext = HaveXkbExtension(resources->display) != 0;

  resources->main_window = ReadWindowID();
  if (resources->main_window == None) {
    Log("Invalid/no window ID in XSCREENSAVER_WINDOW");
    goto fail;
  }

  if (!XQueryTree(resources->display, resources->main_window, &unused_root,
                  &resources->parent_window, &unused_children,
                  &unused_nchildren)) {
    Log("XQueryTree failed for auth window");
    goto fail;
  }
  if (unused_children != NULL) {
    XFree(unused_children);
    unused_children = NULL;
  }
  if (resources->parent_window == None) {
    Log("XQueryTree returned no parent window for auth window");
    goto fail;
  }

  if (!AllocNamedColorOrLog(resources, "background", config->background_color,
                            &resources->xcolor_background,
                            &resources->background_color_allocated)) {
    goto fail;
  }
  if (!AllocNamedColorOrLog(resources, "foreground", config->foreground_color,
                            &resources->xcolor_foreground,
                            &resources->foreground_color_allocated)) {
    goto fail;
  }
  if (!AllocNamedColorOrLog(resources, "warning", config->warning_color,
                            &resources->xcolor_warning,
                            &resources->warning_color_allocated)) {
    goto fail;
  }

  if (config->font_name[0] != '\0') {
    resources->core_font =
        XLoadQueryFont(resources->display, config->font_name);
    have_font = resources->core_font != NULL;
#ifdef HAVE_XFT_EXT
    if (!have_font) {
      resources->xft_font =
          FixedXftFontOpenName(resources->display, screen, config->font_name);
      have_font = resources->xft_font != NULL;
    }
#endif
  }
  if (!have_font) {
    if (config->font_name[0] != '\0') {
      Log("Could not load the specified font %s - trying a default font",
          config->font_name);
    }
#ifdef HAVE_XFT_EXT
    resources->xft_font =
        FixedXftFontOpenName(resources->display, screen, "monospace");
    have_font = resources->xft_font != NULL;
#endif
  }
  if (!have_font) {
    resources->core_font = XLoadQueryFont(resources->display, "fixed");
    have_font = resources->core_font != NULL;
  }
  if (!have_font) {
    Log("Could not load a mind-bogglingly stupid font");
    goto fail;
  }

#ifdef HAVE_XFT_EXT
  if (resources->xft_font != NULL) {
    if (!AllocXftColorOrLog(resources, "foreground",
                            &resources->xcolor_foreground,
                            &resources->xft_color_foreground,
                            &resources->xft_color_foreground_allocated)) {
      goto fail;
    }
    if (!AllocXftColorOrLog(resources, "warning", &resources->xcolor_warning,
                            &resources->xft_color_warning,
                            &resources->xft_color_warning_allocated)) {
      goto fail;
    }
  }
#endif

  SelectMonitorChangeEvents(resources->display, resources->main_window);
  return 1;

fail:
  if (unused_children != NULL) {
    XFree(unused_children);
  }
  AuthUiResourcesCleanup(resources);
  return 0;
}

void AuthUiResourcesCleanup(struct AuthUiResources *resources) {
  if (resources->display == NULL) {
    return;
  }

#ifdef HAVE_XFT_EXT
  if (resources->xft_font != NULL) {
    if (resources->xft_color_warning_allocated) {
      XftColorFree(
          resources->display,
          DefaultVisual(resources->display, DefaultScreen(resources->display)),
          resources->colormap, &resources->xft_color_warning);
      resources->xft_color_warning_allocated = false;
    }
    if (resources->xft_color_foreground_allocated) {
      XftColorFree(
          resources->display,
          DefaultVisual(resources->display, DefaultScreen(resources->display)),
          resources->colormap, &resources->xft_color_foreground);
      resources->xft_color_foreground_allocated = false;
    }
    XftFontClose(resources->display, resources->xft_font);
    resources->xft_font = NULL;
  }
#endif

  if (resources->core_font != NULL) {
    XFreeFont(resources->display, resources->core_font);
    resources->core_font = NULL;
  }

  if (resources->warning_color_allocated) {
    XFreeColors(resources->display, resources->colormap,
                &resources->xcolor_warning.pixel, 1, 0);
    resources->warning_color_allocated = false;
  }
  if (resources->foreground_color_allocated) {
    XFreeColors(resources->display, resources->colormap,
                &resources->xcolor_foreground.pixel, 1, 0);
    resources->foreground_color_allocated = false;
  }
  if (resources->background_color_allocated) {
    XFreeColors(resources->display, resources->colormap,
                &resources->xcolor_background.pixel, 1, 0);
    resources->background_color_allocated = false;
  }

#ifdef HAVE_FONTCONFIG
  FcFini();
#endif

  XCloseDisplay(resources->display);
  resources->display = NULL;
}
