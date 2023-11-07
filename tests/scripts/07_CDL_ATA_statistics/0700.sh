#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="Configuration (show)"

if [ $# == 0 ]; then
        echo $testname
        exit 0
fi

filename=$0
dev=$1

require_ata_dev "${dev}"
require_cdl_statistics "${dev}"

# Show the CDL statistics configuration
echo "# cdladm stats-show ${dev}"
cdladm stats-show "${dev}" | \
	grep -v "High priority enhancement" > /tmp/cdl-stats-show || \
	exit_failed

# Do the same again with force-ata and compare results
echo "# cdladm stats-show --force-ata ${dev}"
cdladm stats-show --force-ata "${dev}" | \
	grep -v "High priority enhancement" > /tmp/cdl-stats-show-ata || \
	exit_failed

diff -q  /tmp/cdl-stats-show /tmp/cdl-stats-show-ata || \
	exit_failed "ATA stats-show and default stats-show files differ"

exit 0
