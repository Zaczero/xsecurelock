/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef XSCREENSAVER_API_H
#define XSCREENSAVER_API_H

#include <X11/X.h>  // for Window

/*! \brief Export the given window ID to the environment for a saver/auth child.
 *
 * This simply sets $XSCREENSAVER_WINDOW.
 *
 * \param w The window the child should draw on.
 */
void ExportWindowID(Window w);

/*! \brief Export the given saver index to the environment for a saver/auth child.
 *
 * This simply sets $XSCREENSAVER_SAVER_INDEX.
 *
 * \param index The index of the saver.
 */
void ExportSaverIndex(int index);

/*! \brief Reads the window ID to draw on from the environment.
 *
 * This simply reads $XSCREENSAVER_WINDOW.
 */
Window ReadWindowID(void);

#endif
