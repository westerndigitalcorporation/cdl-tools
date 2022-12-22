#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL active time (0x0 complete-earliest policy)"
	exit 0
fi

cdl_file="${scriptdir}/cdl/T2A-active-time.cdl"
testname=active-time-complete-earliest
filename=$0
dev=$1

test_setup $dev T2A $cdl_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd=$(fio_common_cmdline $dev $filename $testname)
fiocmd+=" --cmdprio_percentage=100 --cmdprio_class=4 --cmdprio=1"
fiocmd+=" --ramp_time=10"

echo "Running fio:"
fiolog="${logdir}/$(test_num $filename)_fio.log"
echo "${fiocmd}"
eval ${fiocmd} | tee "${fiolog}" || exit_failed " --> FAILED"

# We should not have any IO error
if fio_has_io_error "${fiolog}"; then
	exit_failed " --> FAILED (IO errors detected, we should not have any)"
fi

exit 0
