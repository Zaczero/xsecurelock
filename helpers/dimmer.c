/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*!
 *\brief Screen dimmer.
 *
 *A simple tool to dim the screen, then wait a little so a screen locker can
 *take over.
 *
 *Sample usage:
 *  xset s 300 2
 *  xss-lock -n dim-screen -l xsecurelock
 */

#include "config.h"

#include <X11/X.h>      // for Window, Atom, CopyFromParent, GCForegr...
#include <X11/Xatom.h>  // for XA_CARDINAL
#include <X11/Xlib.h>   // for Display, XColor, XSetWindowAttributes
#include <math.h>       // for pow, ceil, frexp, nextafter, sqrt
#include <stdbool.h>    // for bool
#include <stdint.h>     // for int64_t, uint32_t
#include <stdio.h>      // for snprintf
#include <string.h>     // for memset

#include "../env_settings.h"   // for GetBoolSetting, GetClampedFiniteDouble...
#include "../logging.h"        // for Log
#include "../time_util.h"      // for SleepMs, SleepNs
#include "../wm_properties.h"  // for SetWMProperties
#include "dimmer_bayer.h"      // for DimmerBayerPoint
#include "dimmer_opacity.h"    // for DimmerOpacityFromSrgbAlpha

struct DimmerConfig {
  int dim_time_ms;
  int wait_time_ms;
  double dim_fps;
  double dim_alpha;
  bool have_compositor;
  XColor dim_color;
};

struct DimEffect {
  const struct DimmerConfig *config;
  void (*PreCreateWindow)(void *self, Display *display,
                          XSetWindowAttributes *dimattrs,
                          unsigned long *dimmask);
  void (*PostCreateWindow)(void *self, Display *display, Window dim_window);
  void (*DrawFrame)(void *self, Display *display, Window dim_window, int frame,
                    int w, int h);
  void (*Cleanup)(void *self, Display *display);
  int frame_count;
};

struct DitherEffect {
  struct DimEffect super;
  int pattern_power;
  int pattern_frames;
  int max_fill_size;

  Pixmap pattern;
  XGCValues gc_values;
  GC dim_gc;
  GC pattern_gc;
};

struct OpacityEffect {
  struct DimEffect super;

  Atom property_atom;
  double dim_color_brightness;
};

static int HaveCompositor(Display *display) {
  char buf[64];
  int buflen =
      snprintf(buf, sizeof(buf), "_NET_WM_CM_S%d", (int)DefaultScreen(display));
  if (buflen <= 0 || (size_t)buflen >= sizeof(buf)) {
    Log("Wow, pretty long screen number you got there");
    return 0;
  }
  Atom atom = XInternAtom(display, buf, False);
  return XGetSelectionOwner(display, atom) != None;
}

static void LoadDimmerConfig(Display *display, struct DimmerConfig *config) {
  double default_dim_fps = GetClampedFiniteDoubleSetting(
      "XSECURELOCK_" /* REMOVE IN v2 */ "DIM_MIN_FPS", 60.0, 1.0, 1000.0);

  config->dim_time_ms =
      GetNonnegativeIntSetting("XSECURELOCK_DIM_TIME_MS", 2000);
  config->wait_time_ms =
      GetNonnegativeIntSetting("XSECURELOCK_WAIT_TIME_MS", 5000);
  config->dim_fps = GetClampedFiniteDoubleSetting("XSECURELOCK_DIM_FPS",
                                                  default_dim_fps, 1.0, 1000.0);
  config->dim_alpha =
      GetClampedFiniteDoubleSetting("XSECURELOCK_DIM_ALPHA", 0.875, 0.001, 1.0);
  config->have_compositor = GetBoolSetting(
      "XSECURELOCK_DIM_OVERRIDE_COMPOSITOR_DETECTION", HaveCompositor(display));

  Colormap colormap = DefaultColormap(display, DefaultScreen(display));
  const char *color_name = GetStringSetting("XSECURELOCK_DIM_COLOR", "black");
  if (XParseColor(display, colormap, color_name, &config->dim_color) &&
      XAllocColor(display, colormap, &config->dim_color)) {
    return;
  }

  config->dim_color.pixel = BlackPixel(display, DefaultScreen(display));
  XQueryColor(display, colormap, &config->dim_color);
  Log("Could not allocate color or unknown color name: %s", color_name);
}

static void DitherEffectPreCreateWindow(void *unused_self,
                                        Display *unused_display,
                                        XSetWindowAttributes *unused_dimattrs,
                                        unsigned long *unused_dimmask) {
  (void)unused_self;
  (void)unused_display;
  (void)unused_dimattrs;
  (void)unused_dimmask;
}

static void DitherEffectPostCreateWindow(void *self, Display *display,
                                         Window dim_window) {
  struct DitherEffect *dimmer = self;

  dimmer->gc_values.foreground = 0;
  dimmer->pattern =
      XCreatePixmap(display, dim_window, 1 << dimmer->pattern_power,
                    1 << dimmer->pattern_power, 1);
  dimmer->pattern_gc =
      XCreateGC(display, dimmer->pattern, GCForeground, &dimmer->gc_values);
  XFillRectangle(display, dimmer->pattern, dimmer->pattern_gc, 0, 0,
                 1 << dimmer->pattern_power, 1 << dimmer->pattern_power);
  XSetForeground(display, dimmer->pattern_gc, 1);

  dimmer->gc_values.fill_style = FillStippled;
  dimmer->gc_values.foreground = dimmer->super.config->dim_color.pixel;
  dimmer->gc_values.stipple = dimmer->pattern;
  dimmer->dim_gc =
      XCreateGC(display, dim_window, GCFillStyle | GCForeground | GCStipple,
                &dimmer->gc_values);
}

static void DitherEffectDrawFrame(void *self, Display *display,
                                  Window dim_window, int frame, int w, int h) {
  struct DitherEffect *dimmer = self;
  int start_pframe = frame * dimmer->pattern_frames / dimmer->super.frame_count;
  int end_pframe =
      (frame + 1) * dimmer->pattern_frames / dimmer->super.frame_count;

  for (int pframe = start_pframe; pframe < end_pframe; ++pframe) {
    int x = 0;
    int y = 0;
    DimmerBayerPoint(pframe, dimmer->pattern_power, &x, &y);
    XDrawPoint(display, dimmer->pattern, dimmer->pattern_gc, x, y);
  }

  XChangeGC(display, dimmer->dim_gc, GCStipple, &dimmer->gc_values);
  for (int y = 0; y < h;) {
    int hh = h - y;
    if (hh > dimmer->max_fill_size) {
      hh = dimmer->max_fill_size;
    }
    for (int x = 0; x < w;) {
      int ww = w - x;
      if (ww > dimmer->max_fill_size) {
        ww = dimmer->max_fill_size;
      }
      XFillRectangle(display, dim_window, dimmer->dim_gc, x, y, ww, hh);
      XFlush(display);
      x += ww;
    }
    y += hh;
  }
}

static void DitherEffectCleanup(void *self, Display *display) {
  struct DitherEffect *dimmer = self;

  if (dimmer->dim_gc != NULL) {
    XFreeGC(display, dimmer->dim_gc);
    dimmer->dim_gc = NULL;
  }
  if (dimmer->pattern_gc != NULL) {
    XFreeGC(display, dimmer->pattern_gc);
    dimmer->pattern_gc = NULL;
  }
  if (dimmer->pattern != None) {
    XFreePixmap(display, dimmer->pattern);
    dimmer->pattern = None;
  }
}

static void DitherEffectInit(struct DitherEffect *dimmer,
                             const struct DimmerConfig *config) {
  memset(dimmer, 0, sizeof(*dimmer));
  dimmer->super.config = config;
  dimmer->pattern = None;

  dimmer->pattern_power = 3;
  {
    double total_time_ms = config->dim_time_ms / config->dim_alpha;
    double total_frames_min = total_time_ms / 1000.0 * config->dim_fps;
    (void)frexp(sqrt(total_frames_min), &dimmer->pattern_power);
  }
  if (dimmer->pattern_power < 2) {
    dimmer->pattern_power = 2;
  }
  if (dimmer->pattern_power > 8) {
    dimmer->pattern_power = 8;
  }

  dimmer->pattern_frames =
      (int)ceil(pow(1 << dimmer->pattern_power, 2) * config->dim_alpha);
  dimmer->super.frame_count =
      (int)ceil(config->dim_time_ms * config->dim_fps / 1000.0);
  if (dimmer->super.frame_count < 1) {
    dimmer->super.frame_count = 1;
  }

  {
    int max_fill_size =
        GetClampedIntSetting("XSECURELOCK_DIM_MAX_FILL_SIZE", 2048, 1, 1 << 30);
    int max_fill_patterns = max_fill_size >> dimmer->pattern_power;
    if (max_fill_patterns == 0) {
      max_fill_patterns = 1;
    }
    dimmer->max_fill_size = max_fill_patterns << dimmer->pattern_power;
  }

  dimmer->super.PreCreateWindow = DitherEffectPreCreateWindow;
  dimmer->super.PostCreateWindow = DitherEffectPostCreateWindow;
  dimmer->super.DrawFrame = DitherEffectDrawFrame;
  dimmer->super.Cleanup = DitherEffectCleanup;
}

static void OpacityEffectPreCreateWindow(void *unused_self,
                                         Display *unused_display,
                                         XSetWindowAttributes *dimattrs,
                                         unsigned long *dimmask) {
  const struct OpacityEffect *dimmer = unused_self;
  (void)unused_display;

  dimattrs->background_pixel = dimmer->super.config->dim_color.pixel;
  *dimmask |= CWBackPixel;
}

static void OpacityEffectPostCreateWindow(void *self, Display *display,
                                          Window dim_window) {
  struct OpacityEffect *dimmer = self;
  unsigned long property_value = 0;

  XChangeProperty(display, dim_window, dimmer->property_atom, XA_CARDINAL, 32,
                  PropModeReplace, (const unsigned char *)&property_value, 1);
}

static double sRGBToLinear(double value) {
  return (value <= 0.04045) ? value / 12.92 : pow((value + 0.055) / 1.055, 2.4);
}

static double LinearTosRGB(double value) {
  return (value <= 0.0031308) ? 12.92 * value
                              : (1.055 * pow(value, 1.0 / 2.4)) - 0.055;
}

static void OpacityEffectDrawFrame(void *self, Display *display,
                                   Window dim_window, int frame, int unused_w,
                                   int unused_h) {
  struct OpacityEffect *dimmer = self;
  double linear_alpha = ((frame + 1) * dimmer->super.config->dim_alpha) /
                        dimmer->super.frame_count;
  double linear_min = linear_alpha * dimmer->dim_color_brightness;
  double linear_max =
      (linear_alpha * dimmer->dim_color_brightness) + (1.0 - linear_alpha);
  double srgb_min = LinearTosRGB(linear_min);
  double srgb_max = LinearTosRGB(linear_max);
  double srgb_alpha = 1.0 - (srgb_max - srgb_min);
  uint32_t opacity32 = DimmerOpacityFromSrgbAlpha(srgb_alpha);
  unsigned long property_value = (unsigned long)opacity32;

  (void)unused_w;
  (void)unused_h;
  XChangeProperty(display, dim_window, dimmer->property_atom, XA_CARDINAL, 32,
                  PropModeReplace, (const unsigned char *)&property_value, 1);
  XFlush(display);
}

static void OpacityEffectCleanup(void *unused_self, Display *unused_display) {
  (void)unused_self;
  (void)unused_display;
}

static void OpacityEffectInit(struct OpacityEffect *dimmer, Display *display,
                              const struct DimmerConfig *config) {
  memset(dimmer, 0, sizeof(*dimmer));
  dimmer->super.config = config;
  dimmer->property_atom = XInternAtom(display, "_NET_WM_WINDOW_OPACITY", False);
  dimmer->dim_color_brightness =
      (sRGBToLinear(config->dim_color.red / 65535.0) * 0.2126) +
      (sRGBToLinear(config->dim_color.green / 65535.0) * 0.7152) +
      (sRGBToLinear(config->dim_color.blue / 65535.0) * 0.0722);

  dimmer->super.frame_count =
      (int)ceil(config->dim_time_ms * config->dim_fps / 1000.0);
  if (dimmer->super.frame_count < 1) {
    dimmer->super.frame_count = 1;
  }

  dimmer->super.PreCreateWindow = OpacityEffectPreCreateWindow;
  dimmer->super.PostCreateWindow = OpacityEffectPostCreateWindow;
  dimmer->super.DrawFrame = OpacityEffectDrawFrame;
  dimmer->super.Cleanup = OpacityEffectCleanup;
}

int main(int argc, char **argv) {
  Display *display = NULL;
  Window dim_window = None;
  int status = 1;
  struct DimmerConfig config = {0};
  struct DitherEffect dither_dimmer;
  struct OpacityEffect opacity_dimmer;
  struct DimEffect *dimmer = NULL;

  memset(&dither_dimmer, 0, sizeof(dither_dimmer));
  memset(&opacity_dimmer, 0, sizeof(opacity_dimmer));

  display = XOpenDisplay(NULL);
  if (display == NULL) {
    Log("Could not connect to $DISPLAY");
    goto done;
  }

  LoadDimmerConfig(display, &config);
  if (config.have_compositor) {
    OpacityEffectInit(&opacity_dimmer, display, &config);
    dimmer = &opacity_dimmer.super;
  } else {
    DitherEffectInit(&dither_dimmer, &config);
    dimmer = &dither_dimmer.super;
  }

  {
    Window root_window = DefaultRootWindow(display);
    int w = DisplayWidth(display, DefaultScreen(display));
    int h = DisplayHeight(display, DefaultScreen(display));
    XSetWindowAttributes dimattrs = {0};
    unsigned long dimmask = CWSaveUnder | CWOverrideRedirect;
    if (w <= 0 || h <= 0) {
      Log("Display has invalid size %dx%d", w, h);
      goto done;
    }

    dimattrs.save_under = 1;
    dimattrs.override_redirect = 1;
    dimmer->PreCreateWindow(dimmer, display, &dimattrs, &dimmask);
    dim_window = XCreateWindow(display, root_window, 0, 0, (unsigned int)w,
                               (unsigned int)h, 0, CopyFromParent, InputOutput,
                               CopyFromParent, dimmask, &dimattrs);
    SetWMProperties(display, dim_window, "xsecurelock-dimmer", "dim", argc,
                    argv);
    dimmer->PostCreateWindow(dimmer, display, dim_window);

    {
      int64_t sleep_time_ns =
          ((int64_t)config.dim_time_ms * 1000000) / dimmer->frame_count;
      XMapRaised(display, dim_window);
      for (int frame = 0; frame < dimmer->frame_count; ++frame) {
        dimmer->DrawFrame(dimmer, display, dim_window, frame, w, h);
        if (SleepNs(sleep_time_ns) != 0) {
          Log("Could not sleep between dimming frames");
          goto done;
        }
      }
    }
  }

  if (SleepMs(config.wait_time_ms) != 0) {
    Log("Could not sleep after dimming finished");
    goto done;
  }

  status = 0;

done:
  if (display != NULL && dimmer != NULL && dimmer->Cleanup != NULL) {
    dimmer->Cleanup(dimmer, display);
  }
  if (display != NULL && dim_window != None) {
    XDestroyWindow(display, dim_window);
  }
  if (display != NULL) {
    XCloseDisplay(display);
  }
  return status;
}
