#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="Counter values (dur. guideline, policy 0x0, A:B=1:2)"

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

require_ata_dev "${dev}"
require_cdl_statistics "${dev}"
require_duration_guideline "${dev}"

disable_merges "${dev}"

# Using duration guideline with policy 0x0, we should
# not see any change in the statistics counters.
read_limits="stats"
write_limits=""
cdl_dld=1
expect_error=0
compare_latencies=0
ncq=1
rw="randread"
stats="ATA-stats.cfg"
expect_stat_a_count=0
expect_stat_b_count=0

execute_test "${testname}" \
	"${read_limits}" "${write_limits}" \
	"${cdl_dld}" "${expect_error}" \
	"${compare_latencies}" "${filename}" \
	"${dev}" "${ncq}" "${rw}" \
	"${stats}" "${expect_stat_a_count}" "${expect_stat_b_count}" || \
	exit_failed "error executing fio test"

exit 0
