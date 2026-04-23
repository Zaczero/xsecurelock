/*
 * Copyright 2018 Google Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ENV_SETTINGS_H
#define ENV_SETTINGS_H

/*! \brief Loads an integer setting from the environment.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The value of the setting, or def if unset or not a number.
 */
unsigned long long GetUnsignedLongLongSetting(const char *name,
                                              unsigned long long def);

/*! \brief Loads an integer setting from the environment.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The value of the setting, or def if unset or not a number.
 */
long GetLongSetting(const char *name, long def);

/*! \brief Loads an integer setting from the environment.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The value of the setting, or def if unset or not a number.
 */
int GetIntSetting(const char *name, int def);

/*! \brief Loads an integer setting from the environment and clamps it.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \param min_value The smallest allowed value.
 * \param max_value The largest allowed value.
 * \return The parsed value clamped into the inclusive [min_value, max_value]
 *   range.
 */
int GetClampedIntSetting(const char *name, int def, int min_value,
                         int max_value);

/*! \brief Loads an integer setting from the environment as a boolean.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return 0 if the parsed value is zero, otherwise 1.
 */
int GetBoolSetting(const char *name, int def);

/*! \brief Loads an integer setting from the environment and clamps it to zero
 *   or greater.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The parsed value clamped into the inclusive [0, INT_MAX] range.
 */
int GetNonnegativeIntSetting(const char *name, int def);

/*! \brief Loads an integer setting from the environment and clamps it to one
 *   or greater.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The parsed value clamped into the inclusive [1, INT_MAX] range.
 */
int GetPositiveIntSetting(const char *name, int def);

/*! \brief Loads a floating-point setting from the environment.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The value of the setting, or def if unset or not a number.
 */
double GetDoubleSetting(const char *name, double def);

/*! \brief Loads a finite floating-point setting from the environment.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The value of the setting, or def if unset, invalid, or non-finite.
 */
double GetFiniteDoubleSetting(const char *name, double def);

/*! \brief Loads a finite floating-point setting from the environment and clamps
 *   it.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \param min_value The smallest allowed value.
 * \param max_value The largest allowed value.
 * \return The parsed value, or default value, clamped into the inclusive
 *   [min_value, max_value] range.
 */
double GetClampedFiniteDoubleSetting(const char *name, double def,
                                     double min_value, double max_value);

/*! \brief Loads a setting from the environment.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \return The value of the setting, or def if unset or empty.
 */
const char *GetStringSetting(const char *name, const char *def);

/*! \brief Loads a setting from the environment that specifies a binary name.
 *
 * \param name The setting to read (with XSECURELOCK_ variable name prefix).
 * \param def The default value.
 * \param is_auth If the path should be an auth child.
 * \return The value of the setting, or def if unset, invalid or empty.
 */
const char *GetExecutablePathSetting(const char *name, const char *def,
                                     int is_auth);

#endif
