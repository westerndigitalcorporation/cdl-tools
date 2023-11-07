#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2023 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

testname="Configuration (upload empty conf.)"

if [ $# == 0 ]; then
        echo $testname
        exit 0
fi

filename=$0
dev=$1

require_ata_dev "${dev}"
require_cdl_statistics "${dev}"

statscfg="${scriptdir}/cdl/ATA-stats-empty.cfg"

function upload_stats()
{
	local opts="$1"

	echo "# cdladm stats-upload ${opts} --file ${statscfg} ${dev}"
	cdladm stats-upload ${opts} --file "${statscfg}" "${dev}" || \
		exit_failed

	echo "cdladm stats-save ${opts} --file /tmp/cdl-stats-save ${dev}"
	cdladm stats-save ${opts} --file /tmp/cdl-stats-save "${dev}" || \
		exit_failed

	diff -q /tmp/cdl-stats-save "${statscfg}" || \
		exit_failed "Saved stats configuration differ from upladed configuration"
}

# Upload empty statistics and check the upload using save command
upload_stats

# Same using --force-ata
upload_stats "--force-ata"

exit 0
