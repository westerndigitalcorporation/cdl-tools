#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL active time (0xd complete-unavailable policy)"
	exit 0
fi

cdl_file="${scriptdir}/cdl/T2A-active-time.cdl"
testname=active-time-complete-unavailable
filename=$0
dev=$1

if dev_has_bad_fw "$1"; then
	exit_skip
fi

test_setup $dev T2A $cdl_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd=$(fio_common_cmdline $dev $filename $testname)
fiocmd+=" --cmdprio_percentage=100 --cmdprio_class=4 --cmdprio=2"

echo "Running fio:"
fiolog="${logdir}/$(test_num $filename)_fio.log"
echo "${fiocmd}"
eval ${fiocmd} | tee "${fiolog}" || exit_failed " --> FAILED"

err="$(grep ", err=" ${fiolog})"
if [ -z "${err}" ]; then
        exit_failed " --> FAILED (fio did not see any IO error)"
fi

errno="$(echo "${err}" | cut -d'=' -f3 | cut -d'/' -f1)"
if [ "${errno}" != "62" ]; then
        exit_failed " --> FAILED (fio saw error ${errno} instead of ETIME=62)"
fi

exit 0
