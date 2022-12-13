#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL (enable/disable)"
	exit 0
fi

# Enable CDL (do it twice)
echo "Enable CDL..."
enable_cdl "$1"

enabled=$(cdl_enabled "$1")
if [ "${enabled}" != "1" ]; then
	exit_failed " --> enable CDL FAILED"
fi

echo "Enable CDL (again)..."
enable_cdl "$1"

enabled=$(cdl_enabled "$1")
if [ "${enabled}" != "1" ]; then
	exit_failed " --> Enable CDL FAILED"
fi

# Check with cdladm
count="$(cdladm info "$1" | grep -c "Command duration limits: supported, enabled")"
if [ "${count}" != 2 ]; then
	exit_failed " --> cdladm FAILED to detect CDL enabled state"
fi

# Disable CDL (do it twice)
echo "Disable CDL..."
disable_cdl "$1"

enabled=$(cdl_enabled "$1")
if [ "${enabled}" != "0" ]; then
	exit_failed " --> Disable CDL FAILED"
fi

echo "Disable CDL..."
disable_cdl "$1"

enabled=$(cdl_enabled "$1")
if [ "${enabled}" != "0" ]; then
	exit_failed " --> Disable CDL FAILED"
fi

# Check with cdladm
count="$(cdladm info "$1" | grep -c "Command duration limits: supported, disabled")"
if [ "${count}" != 2 ]; then
	exit_failed " --> cdladm FAILED to detect CDL disabled state"
fi

exit 0
