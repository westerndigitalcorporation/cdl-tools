#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL active time (0xf abort policy)"
T2A_file="${scriptdir}/cdl/T2A-active-time.cdl"
T2B_file="${scriptdir}/cdl/T2B-empty.cdl"
cdl_dld=4
compare_latencies=0
expect_error=1

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

test_setup $dev $T2A_file $T2B_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd=$(fio_common_cmdline $dev $filename "$testname" $cdl_dld $compare_latencies)

echo "Running fio:"
fiolog="${logdir}/$(test_num $filename)_fio.log"
echo "${fiocmd}"
eval ${fiocmd} | tee "${fiolog}" || exit_failed " --> FAILED"

analyze_log $fiolog $expect_error $compare_latencies $cdl_dld || \
	exit_failed " --> FAILED (error during analyze log)"

exit 0
