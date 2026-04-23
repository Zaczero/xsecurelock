/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef MONITORS_H
#define MONITORS_H

#include <X11/X.h>     // for Window
#include <X11/Xlib.h>  // for Display
#include <stddef.h>    // for size_t

typedef struct {
  int x, y, width, height;
} Monitor;

/*! \brief Queries the current monitor configuration.
 *
 * Note: out_monitors will be zero padded and sorted in some deterministic order
 * so memcmp can be used to check if the monitor configuration has actually
 * changed.
 *
 * \param dpy The current display.
 * \param w The window this application intends to draw in.
 * \param out_monitors A pointer to an array that will receive the monitor
 *   configuration (in coordinates relative and clipped to the window w.
 * \param max_monitors The size of the array.
 * \return The number of monitors returned in the array.
 */
size_t GetMonitors(Display* dpy, Window window, Monitor* out_monitors,
                   size_t max_monitors);

/*! \brief Enable receiving monitor change events for the given display at w.
 */
void SelectMonitorChangeEvents(Display* dpy, Window window);

/*! \brief Returns the event type that indicates a change to the monitor
 *    configuration.
 *
 * \param dpy The current display.
 * \param type The received event type.
 *
 * \returns 1 if the received event is a monitor change event and GetMonitors
 *   should be called, or 0 otherwise.
 */
int IsMonitorChangeEvent(Display* dpy, int type);

#endif
