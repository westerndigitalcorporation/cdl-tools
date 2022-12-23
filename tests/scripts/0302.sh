#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL dur. guideline (0x2 continue-no-limit policy)"
T2A_file="${scriptdir}/cdl/T2A-duration-guideline.cdl"
T2B_file="${scriptdir}/cdl/T2B-empty.cdl"

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

test_setup $dev $T2A_file $T2B_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd=$(fio_common_cmdline $dev $filename "$testname")
fiocmd+=" --cmdprio_percentage=10 --cmdprio_class=4 --cmdprio=4"
fiocmd+=" --ramp_time=10"

echo "Running fio:"
fiolog="${logdir}/$(test_num $filename)_fio.log"
echo "${fiocmd}"
eval ${fiocmd} | tee "${fiolog}" || exit_failed " --> FAILED"

# We should not have any IO error
if fio_has_io_error "${fiolog}"; then
	exit_failed " --> FAILED (IO errors detected, we should not have any)"
fi

#
# Get IO completion average latencies and issue a warning if we do not
# detect sufficient difference between cdl vs no-cdl latencies
#
nocdl=$(fio_get_clat_avg "${fiolog}" "0/0")
cdl=$(fio_get_clat_avg "${fiolog}" "4/1")

echo "Average latency: no-cdl=${nocdl}, cdl=${cdl}"

thresh=$(( nocdl / 2 ))
if [ ${cdl} -gt ${thresh} ]; then
	exit_warning " --> WARNING: bad average latency"
fi

exit 0
