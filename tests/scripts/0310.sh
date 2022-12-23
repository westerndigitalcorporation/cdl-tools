#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL active time (0x0 complete-earliest policy)"
T2A_file="${scriptdir}/cdl/T2A-active-time.cdl"
T2B_file="${scriptdir}/cdl/T2B-empty.cdl"

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

test_setup $dev $T2A_file $T2B_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd=$(fio_common_cmdline $dev $filename "$testname")
fiocmd+=" --cmdprio_percentage=100 --cmdprio_class=4 --cmdprio=1"

echo "Running fio:"
fiolog="${logdir}/$(test_num $filename)_fio.log"
echo "${fiocmd}"
eval ${fiocmd} | tee "${fiolog}" || exit_failed " --> FAILED"

# We should not have any IO error
if fio_has_io_error "${fiolog}"; then
	exit_failed " --> FAILED (IO errors detected, we should not have any)"
fi

exit 0
