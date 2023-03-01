#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL sysfs (all attributes present)"
	exit 0
fi

dev="$(realpath $1)"
bdev="$(basename ${dev})"

# Check for the presence of the cdl attributes
sysfsdir="/sys/block/${bdev}/device"

echo "Checking ${sysfsdir}/cdl_supported"

if [ ! -f "${sysfsdir}/cdl_supported" ]; then
	exit_failed " --> FAILED (missing cdl_supported attribute)"
fi

echo "Checking ${sysfsdir}/cdl_enable"

if [ ! -f "${sysfsdir}/cdl_enable" ]; then
	exit_failed " --> FAILED (missing cdl_enable attribute)"
fi

exit 0
