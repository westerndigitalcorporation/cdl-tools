#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL active time (0xe abort-recovery policy) writes ncq=off"

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

# While ACTIVE TIME LIMIT POLICY with value 0xE is valid for SCSI.
# ACTIVE TIME LIMIT POLICY with value 0xE is reserved for ATA.
if dev_is_ata "$dev"; then
	exit_skip
fi

T2A_file="${scriptdir}/cdl/T2A-empty.cdl"
T2B_file="${scriptdir}/cdl/T2B-active-time.cdl"
cdl_dld=3
expect_error=1
compare_latencies=0
ncq=0
rw="randwrite"

execute_test "${testname}" \
	"${T2A_file}" "${T2B_file}" \
	"${cdl_dld}" "${expect_error}" \
	"${compare_latencies}" "${filename}" \
	"${dev}" "${ncq}" "${rw}" || \
	exit_failed "error executing fio test"

exit 0
