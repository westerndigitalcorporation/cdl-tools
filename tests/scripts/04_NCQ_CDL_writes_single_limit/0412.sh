#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="CDL active time (0xe abort-recovery policy) writes"

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

read_limits=""
write_limits="active-time"
cdl_dld=3
expect_error=1
compare_latencies=0
ncq=1
rw="randwrite"

execute_test "${testname}" \
	"${read_limits}" "${write_limits}" \
	"${cdl_dld}" "${expect_error}" \
	"${compare_latencies}" "${filename}" \
	"${dev}" "${ncq}" "${rw}" || \
	exit_failed "error executing fio test"

exit 0
