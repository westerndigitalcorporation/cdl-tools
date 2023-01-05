#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL dur. guideline (0x2 continue-no-limit policy) writes"
T2A_file="${scriptdir}/cdl/T2A-empty.cdl"
T2B_file="${scriptdir}/cdl/T2B-duration-guideline.cdl"
cdl_dld=4
expect_error=0
compare_latencies=1
ncq=1
rw=randwrite

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

# Check if supported
have_dg="$(cdladm info "$1" | grep -c "Command duration guidelines: supported")"
if [ "${have_dg}" == "0" ]; then
	exit_skip
fi

execute_test "$testname" $T2A_file $T2B_file $cdl_dld $expect_error $compare_latencies $filename $dev $ncq $rw || \
	exit_failed " --> FAILED (error executing test)"

exit 0