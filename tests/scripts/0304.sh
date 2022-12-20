#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

cdl_file="${scriptdir}/cdl/T2A-duration-guideline.cdl"

if [ $# == 0 ]; then
	echo "CDL dur. guideline (0xf abort policy)"
	exit 0
fi

if dev_has_bad_fw "$1"; then
	exit_skip
fi

# Check if supported
have_dg="$(cdladm info "$1" | grep -c "Command duration guidelines: supported")"
if [ "${have_dg}" == "0" ]; then
	exit_skip
fi

test_setup $1 T2A $cdl_file || \
	exit_failed " --> FAILED (error during setup)"

# fio command
fiocmd="fio --name=duration-guideline-abort"
fiocmd+=" --filename=$1"
fiocmd+=" --random_generator=tausworthe64"
fiocmd+=" --continue_on_error=none"
fiocmd+=" --write_lat_log=${logdir}/$(test_num $0)_lat.log"
fiocmd+=" --log_prio=1 --per_job_logs=0"
fiocmd+=" --rw=randread --ioengine=libaio --iodepth=4"
fiocmd+=" --bs=2M --direct=1"
fiocmd+=" --cmdprio_percentage=100 --cmdprio_class=4 --cmdprio=6"
fiocmd+=" --runtime=$(fio_run_time)"

echo "Running fio:"
fiolog="${logdir}/$(test_num $0)_fio.log"
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
