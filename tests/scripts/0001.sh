#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "cdladm (get device information)"
	exit 0
fi

echo "# cdladm info $1"
cdladm info $1 || exit_failed

exit 0
