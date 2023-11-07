#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="Configuration (save)"

if [ $# == 0 ]; then
        echo $testname
        exit 0
fi

filename=$0
dev=$1

require_ata_dev "${dev}"
require_cdl_statistics "${dev}"

# Save CDL statistics configuration
echo "# cdladm stats-save ${dev}"
cdladm stats-save --file /tmp/cdl-stats-save "${dev}" || \
	exit_failed

# Do the same again with --force-ata and compare the results.
echo "# cdladm stats-save --force-ata ${dev}"
cdladm stats-save --force-ata \
	--file /tmp/cdl-stats-save-ata "${dev}" || \
	exit_failed

diff -q  /tmp/cdl-stats-save /tmp/cdl-stats-save-ata || \
	exit_failed "ATA stats-save and default stats-save files differ"

exit 0
