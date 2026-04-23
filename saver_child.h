/*
 * Copyright 2014 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef SAVER_CHILD_H
#define SAVER_CHILD_H

#include <X11/X.h>     // for Window
#include <X11/Xlib.h>  // for Display

#define MAX_SAVERS 16

/*! \brief Kill all saver children.
 *
 * This can be used from a signal handler.
 */
void KillAllSaverChildren(int signo);
void KillAllSaverChildrenSigHandler(int signo);

/*! \brief Starts or stops the screen saver child process.
 *
 * \param dpy The X11 display.
 * \param w The screen saver window. Will get cleared after saver child
 *   execution.
 * \param index The index of the saver to maintain (0 <= index < MAX_SAVERS).
 * \param executable What binary to spawn for screen saving. No arguments will
 *   be passed.
 * \param should_be_running If true, the saver child is started if not running
 *   yet; if alse, the saver child will be terminated.
 */
void WatchSaverChild(Display *dpy, Window w, int index, const char *executable,
                     int should_be_running);

#endif
