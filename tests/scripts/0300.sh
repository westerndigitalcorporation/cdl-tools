#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL dur. guideline (0x0 complete-earliest policy)"
	exit 0
fi

# Check if supported
have_dg="$(cdladm info "$1" | grep -c "Command duration guidelines: supported")"
if [ "${have_dg}" == "0" ]; then
	exit_skip
fi

echo "Uploading T2A page"
cdladm upload --file "${scriptdir}/cdl/T2A-duration-guideline.cdl" "$1" || \
	exit_failed " --> FAILED to upload T2A page"

enable_cdl "$1" || exit_failed " --> FAILED to enable CDL"

# fio command
fiocmd="fio --name=duration-guideline-best-effort"
fiocmd+=" --filename=$1"
fiocmd+=" --random_generator=tausworthe64"
fiocmd+=" --continue_on_error=none"
fiocmd+=" --write_lat_log=${logdir}/$(test_num $0)_lat.log"
fiocmd+=" --log_prio=1 --per_job_logs=0"
fiocmd+=" --rw=randread --ioengine=libaio --iodepth=32"
fiocmd+=" --bs=128k --direct=1"
fiocmd+=" --cmdprio_percentage=10 --cmdprio_class=4 --cmdprio=1"
fiocmd+=" --ramp_time=10 --runtime=60"

echo "Running fio:"
fiolog="${logdir}/$(test_num $0)_fio.log"
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
