#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "cdladm (bad devices)"
	exit 0
fi

if ! have_dev "console" -a have_module "null_blk"; then
	exit_skip
fi

# Not a block device case
if have_dev "console"; then
	echo "# Trying /dev/console"
	cdladm info "/dev/console" && exit_failed " --> SUCCESS (should FAIL)"
	echo ""
fi

if have_module "null_blk"; then
	# Block device without CDL support
	nulldev=$(create_nullb)
	nullb="/dev/nullb${nulldev}"

	echo "# cdladm info ${nullb}"
	cdladm info "${nullb}"
	if [ $? == 0 ]; then
		destroy_nullb "${nulldev}"
		exit_failed " --> SUCCESS (should FAIL)"
	fi

	destroy_nullb "${nulldev}"
fi

exit 0
