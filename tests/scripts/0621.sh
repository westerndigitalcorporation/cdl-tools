#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL inactive time (0xd complete-unavailable policy) writes ncq=off"
T2A_file="${scriptdir}/cdl/T2A-empty.cdl"
T2B_file="${scriptdir}/cdl/T2B-inactive-time.cdl"
cdl_dld=2
expect_error=1
compare_latencies=0
ncq=0
rw=randwrite

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

if dev_has_bad_fw "$1"; then
	exit_skip
fi

execute_test "$testname" $T2A_file $T2B_file $cdl_dld $expect_error $compare_latencies $filename $dev $ncq $rw || \
	exit_failed " --> FAILED (error executing test)"

exit 0