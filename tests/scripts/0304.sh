#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL dur. guideline (0xf abort policy)"
T2A_file="${scriptdir}/cdl/T2A-duration-guideline.cdl"
T2B_file="${scriptdir}/cdl/T2B-empty.cdl"
cdl_dld=6
compare_latencies=0

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

if dev_has_bad_fw "$1"; then
	exit_skip
fi

# Check if supported
have_dg="$(cdladm info "$1" | grep -c "Command duration guidelines: supported")"
if [ "${have_dg}" == "0" ]; then
	exit_skip
fi

test_setup $dev $T2A_file $T2B_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd=$(fio_common_cmdline $dev $filename "$testname" $cdl_dld $compare_latencies)

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
