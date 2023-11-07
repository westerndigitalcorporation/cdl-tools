#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="Configuration (reset stat. with read-then-initialize)"

if [ $# == 0 ]; then
	echo $testname
	exit 0
fi

filename=$0
dev=$1

require_ata_dev "${dev}"
require_cdl_statistics "${dev}"
require_duration_guideline "${dev}"

read_limits="stats-active-time"
write_limits=""
cdl_dld=4
expect_error=1
compare_latencies=0
ncq=1
rw="randread"
stats="ATA-stats-active-time.cfg"
expect_stat_a_count=0
expect_stat_b_count=1

execute_test "${testname}" \
	"${read_limits}" "${write_limits}" \
	"${cdl_dld}" "${expect_error}" \
	"${compare_latencies}" "${filename}" \
	"${dev}" "${ncq}" "${rw}" \
	"${stats}" "${expect_stat_a_count}" "${expect_stat_b_count}" || \
	exit_failed "error executing fio test"

# Make sure we have non-zero statistics
check_ata_non_zero_stats_val "${dev}"

# Disable and re-enable CDL to reset statistics
echo "Resetting CDL statistics"
cdladm stats-reset "${dev}"

check_ata_zero_stats_val "${dev}"

echo "All CDL statistics are 0"

exit 0
