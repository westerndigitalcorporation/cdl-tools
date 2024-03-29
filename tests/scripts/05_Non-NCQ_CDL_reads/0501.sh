#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL dur. guideline (0x1 continue-next-limit policy) reads ncq=off"

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

require_duration_guideline "${dev}"

read_limits="duration-guideline"
write_limits=""
cdl_dld=2
expect_error=0
compare_latencies=0
ncq=0
rw="randread"

execute_test "${testname}" \
	"${read_limits}" "${write_limits}" \
	"${cdl_dld}" "${expect_error}" \
	"${compare_latencies}" "${filename}" \
	"${dev}" "${ncq}" "${rw}" || \
	exit_failed "error executing fio test"

exit 0
