#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="Configuration (upload limits then upload non-empty conf.)"

if [ $# == 0 ]; then
        echo $testname
        exit 0
fi

filename=$0
dev=$1

require_ata_dev "${dev}"
require_cdl_statistics "${dev}"

emptystatscfg="${scriptdir}/cdl/ATA-stats-empty.cfg"
statscfg="${scriptdir}/cdl/ATA-stats.cfg"
cdlt2acfg="${scriptdir}/cdl/T2A-stats.cdl"
cdlt2bcfg="${scriptdir}/cdl/T2B-stats.cdl"

function upload_cdl_then_stats()
{
	local opts="$1"

	# Clear the stats first
	echo "# cdladm stats-upload --file ${emptystatscfg} ${dev}"
	cdladm stats-upload ${opts} --file "${emptystatscfg}" "${dev}" || \
		exit_failed

	echo "# cdladm upload --file ${cdlt2acfg} ${dev}"
	cdladm upload ${opts} --file "${cdlt2acfg}" "${dev}" || \
		exit_failed "Upload CDL page ${cdlt2acfg} failed"

	echo "# cdladm stats-upload --file ${statscfg} ${dev}"
	cdladm stats-upload ${opts} --file "${statscfg}" "${dev}" || \
		exit_failed

	echo "# cdladm stats-save --file /tmp/cdl-stats-save ${dev}"
	cdladm stats-save ${opts} --file /tmp/cdl-stats-save "${dev}" || \
		exit_failed

	diff -q /tmp/cdl-stats-save "${statscfg}" || \
		exit_failed "Saved stats configuration differ after T2A page upload"

	echo "# cdladm upload --file ${cdlt2bcfg} ${dev}"
	cdladm upload ${opts} --file "${cdlt2bcfg}" "${dev}" || \
		exit_failed "Upload CDL page ${cdlt2bcfg} failed"

	echo "# cdladm stats-save --file /tmp/cdl-stats-save ${dev}"
	cdladm stats-save ${opts} --file /tmp/cdl-stats-save "${dev}" || \
		exit_failed

	diff -q /tmp/cdl-stats-save "${statscfg}" || \
		exit_failed "Saved stats configuration differ after T2B page upload"
}



# Upload empty statistics and CDL pages and check the configuration
upload_cdl_then_stats

# Same using --force-ata
upload_cdl_then_stats "--force-ata"

exit 0
