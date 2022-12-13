#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL inactive time (0xd complete-unavailable policy)"
	exit 0
fi

echo "Uploading T2A page"
cdladm upload --file "${scriptdir}/cdl/T2A-inactive-time.cdl" "$1" || \
	exit_failed " --> FAILED to upload T2A page"

enable_cdl "$1" || exit_failed " --> FAILED to enable CDL"

# fio command
fiocmd="fio --name=inactive-time-complete-unavailable"
fiocmd+=" --filename=$1"
fiocmd+=" --random_generator=tausworthe64"
fiocmd+=" --continue_on_error=none"
fiocmd+=" --write_lat_log=${logdir}/$(test_num $0)_lat.log"
fiocmd+=" --log_prio=1 --per_job_logs=0"
fiocmd+=" --rw=randread --ioengine=libaio --iodepth=32"
fiocmd+=" --bs=512k --direct=1"
fiocmd+=" --cmdprio_percentage=100 --cmdprio_class=4 --cmdprio=2"
fiocmd+=" --runtime=60"

echo "Running fio:"
fiolog="${logdir}/$(test_num $0)_fio.log"
echo "${fiocmd}"
eval ${fiocmd} | tee "${fiolog}" || exit_failed " --> FAILED"

# We should have IO errors
if ! fio_has_io_error "${fiolog}"; then
	exit_failed " --> FAILED (fio did not see any IO error)"
fi

# IO errors should be 62 (ETIME)
errno="$(fio_get_io_error ${fiolog})"
if [ "${errno}" != "62" ]; then
	exit_failed " --> FAILED (fio saw error ${errno} instead of ETIME=62)"
fi

exit 0
