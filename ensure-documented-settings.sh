#!/bin/sh
#
# Copyright 2014 Google Inc. All rights reserved.
# SPDX-License-Identifier: Apache-2.0

# Simple script to ensure all settings variables are documented.

# List all public settings from real configuration call sites and helper
# scripts. This intentionally ignores header guards, tests, and other internal
# XSECURELOCK_* tokens that are not user-facing settings.
all_settings=$(
	{
		for file in *.c helpers/*.c; do
			[ -f "$file" ] || continue
			<"$file" perl -0ne '
				print "$1\n" while /\bGet(?:UnsignedLongLong|Long|Int|ClampedInt|Bool|NonnegativeInt|PositiveInt|Double|FiniteDouble|ClampedFiniteDouble|String|ExecutablePath|KeySym)Setting\(\s*"((?:XSECURELOCK_[A-Za-z0-9_%]+))"/g;
				print "$1\n" while /"(XSECURELOCK_KEY_%s_COMMAND)"/g;
			'
		done
		for file in helpers/*.in helpers/saver_blank; do
			[ -f "$file" ] || continue
			<"$file" perl -ne '
				print "$1\n" while /\b(XSECURELOCK_[A-Za-z0-9_%]+)\b/g;
			'
		done
	} | sort -u
)

# List of internal settings. These shall not be documented.
internal_settings='
XSECURELOCK_INSIDE_SAVER_MULTIPLEX
'

# List of deprecated settings. These shall not be documented.
deprecated_settings='
XSECURELOCK_PARANOID_PASSWORD
XSECURELOCK_SHOW_LOCKS_AND_LATCHES
XSECURELOCK_WANT_FIRST_KEYPRESS
'

public_settings=$(
	{
		echo "$all_settings"
		echo "$internal_settings"
		echo "$internal_settings"
		echo "$deprecated_settings"
		echo "$deprecated_settings"
	} | sort | uniq -u
)

# List all documented settings.
documented_settings=$(
	<README.md perl -ne '
		if (/ENV VARIABLES START/../ENV VARIABLES END/) {
			print "$_\n" for /^\* +\`(XSECURELOCK_[A-Za-z0-9_%]+)\`/g;
		}
	' | sort -u
)

status=0

undocumented_settings=$(
	{
		echo "$public_settings"
		echo "$documented_settings"
		echo "$documented_settings"
	} | sort | uniq -u
)
if [ -n "$undocumented_settings" ]; then
	echo "The following settings lack documentation:"
	echo "$undocumented_settings"
	echo
	status=1
fi

gone_settings=$(
	{
		echo "$public_settings"
		echo "$public_settings"
		echo "$documented_settings"
	} | sort | uniq -u
)
if [ -n "$gone_settings" ]; then
	echo "The following documented settings don't exist:"
	echo "$gone_settings"
	echo
	status=1
fi

exit $status
