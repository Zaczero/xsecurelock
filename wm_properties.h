/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef WM_PROPERTIES_H
#define WM_PROPERTIES_H

#include <X11/Xlib.h>

/*! \brief Configures properties on the given window for easier debugging.
 *
 * \param dpy The X11 dpy.
 * \param w The window (which shouldn't be mapped yet).
 * \param res_class The class name the window should receive (becomes
 *   WM_CLASS.res_class)
 * \param res_name The window name the window should receive (becomes
 *   WM_CLASS.res_name and WM_NAME)
 * \param argc The number of arguments the main program received.
 * \param argv The arguments the main program received (becomes WM_COMMAND).
 */
void SetWMProperties(Display *dpy, Window w, const char *res_class,
                     const char *res_name, int argc, char *const *argv);

#endif
