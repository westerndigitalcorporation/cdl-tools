#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "cdladm (list CDL descriptors)"
	exit 0
fi

echo "# cdladm list $1"
cdladm list $1 | \
	grep -v "High priority enhancement: " | \
	grep -v "Statistics: " | \
	tee /tmp/cdl-list || \
		exit_failed

if dev_is_ata "$1"; then
	echo "# cdladm list --force-ata $1"
	cdladm list --force-ata $1 | \
		grep -v "High priority enhancement: " | \
		grep -v "Statistics: " | \
		tee /tmp/cdl-list-ata || \
			exit_failed

	diff /tmp/cdl-list /tmp/cdl-list-ata || \
		exit_failed "ata list differs from SAT list"
fi

exit 0
