#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "cdladm (upload CDL descriptors)"
	exit 0
fi

function page_type()
{
	var=$(grep -c T2A "$1")
	if [ "${var}" != 0 ]; then
		echo "T2A"
	else
		echo "T2B"
	fi
}

function upload_page()
{
	local dev="$1"
	local page="$2"
	local opts=""

	echo "=== Uploading and checking page ${page}"

	if [ $# == 3 ]; then
		opts="$3"
	fi

	# Upload the page
	echo "cdladm upload ${opts} --file ${page} ${dev}"
	cdladm upload ${opts} --file ${page} ${dev}

	# Download (save) the page and compare
	echo "Checking uploaded page"
	cdlp=$(page_type ${page} )
	cdladm save ${opts} --page "${cdlp}" --file /tmp/page.cdl "${dev}" || \
		exit_failed " --> FAILED to save ${cdlp} page"
	grep -v "#" /tmp/page.cdl > /tmp/saved_page.cdl
	grep -v "#" ${page} > /tmp/uploaded_page.cdl

	diff -q /tmp/uploaded_page.cdl /tmp/saved_page.cdl || \
		exit_failed " --> FAILED (downloaded page differs from uploaded page)"

	echo "==="
	echo ""
}

function upload_page_ata()
{
	upload_page "$1" "$2" "--force-ata"
}

upload_page "$1" "${scriptdir}/cdl/T2A-duration-guideline.cdl" || \
	exit_failed " --> FAILED to upload T2A duration guideline page"

upload_page "$1" "${scriptdir}/cdl/T2A-active-time.cdl" || \
	exit_failed " --> FAILED to upload T2A active time page"

upload_page "$1" "${scriptdir}/cdl/T2A-inactive-time.cdl" || \
	exit_failed " --> FAILED to upload T2A inactive time page"

upload_page "$1" "${scriptdir}/cdl/T2B.cdl" || \
	exit_failed " --> FAILED to upload T2B page"

if dev_is_ata "$1"; then
	upload_page_ata "$1" "${scriptdir}/cdl/T2A-duration-guideline.cdl" || \
		exit_failed " --> FAILED to upload T2A duration guideline page"

	upload_page_ata "$1" "${scriptdir}/cdl/T2A-active-time.cdl" || \
		exit_failed " --> FAILED to upload T2A active time page"

	upload_page_ata "$1" "${scriptdir}/cdl/T2A-inactive-time.cdl" || \
		exit_failed " --> FAILED to upload T2A inactive time page"

	upload_page_ata "$1" "${scriptdir}/cdl/T2B.cdl" || \
		exit_failed " --> FAILED to upload T2B page"
fi

exit 0


