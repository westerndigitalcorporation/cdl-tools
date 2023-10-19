#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "cdladm (show CDL descriptors)"
	exit 0
fi

echo "# cdladm show $1"
cdladm show "$1" |
	grep -v "High priority enhancement: " | \
	grep -v "Statistics: " | \
	tee /tmp/cdl-show || \
		exit_failed

echo "# cdladm show --page T2A $1"
cdladm show --page T2A "$1" |
	grep -v "High priority enhancement: " | \
	grep -v "Statistics: " | \
	tee /tmp/cdl-show-t2a || \
		exit_failed

echo "# cdladm show --page T2B $1"
cdladm show --page T2B "$1" |
	grep -v "High priority enhancement: " | \
	grep -v "Statistics: " | \
	tee /tmp/cdl-show-t2b || \
		exit_failed

if dev_is_ata "$1"; then
	echo "# cdladm show --force-ata $1"
	cdladm show --force-ata "$1" |
		grep -v "High priority enhancement: " | \
		grep -v "Statistics: " | \
		tee /tmp/cdl-show-ata || \
			exit_failed

	diff -q  /tmp/cdl-show /tmp/cdl-show-ata || \
		exit_failed "ata show differs from SAT show"

	echo "# cdladm show --force-ata --page T2A $1"
	cdladm show --force-ata --page T2A "$1" |
		grep -v "High priority enhancement: " | \
		grep -v "Statistics: " | \
		tee /tmp/cdl-show-t2a-ata || \
			exit_failed

	diff -q  /tmp/cdl-show-t2a /tmp/cdl-show-t2a-ata || \
		exit_failed "T2A ata show differs from SAT show"

	echo "# cdladm show --force-ata --page T2B $1"
	cdladm show --force-ata --page T2B "$1" |
		grep -v "High priority enhancement: " | \
		grep -v "Statistics: " | \
		tee /tmp/cdl-show-t2b-ata || \
			exit_failed

	diff -q  /tmp/cdl-show-t2b /tmp/cdl-show-t2b-ata || \
		exit_failed "T2B ata show differs from SAT show"
fi

exit 0
