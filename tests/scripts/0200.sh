#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

. "${scriptdir}/test_lib"

if [ $# == 0 ]; then
	echo "CDL sysfs (all attributes present)"
	exit 0
fi

dev="$(realpath $1)"
bdev="$(basename ${dev})"

function check_rw()
{
	local rw="$(basename "$1")"

	echo "Checking $1"

	if [ ! -d "$1" ]; then
		exit_failed " --> FAILED (missing ${rw} directory)"
	fi

	if [ ! -f "$1/page" ]; then
		exit_failed " --> FAILED (missing ${rw} page attribute)"
	fi

	if [ "${rw}" == "read" ]; then
		type="READ"
	else
		type="WRITE"
	fi

	cdlp="$(cat "$1/page")"
	str="$(cdladm list "${dev}" | grep "${type}")"
	types=(${str})

	if [ "${cdlp}" != "${types[0]}" ]; then
		exit_failed " --> FAILED (cdladm and sysfs page types differ for $1)"
	fi
}

function check_desc()
{
	local rw="$(basename "$1")"

	dir="$1/$2"
	if [ ! -d "${dir}" ]; then
		exit_failed " --> FAILED (missing directory for ${rw} descriptor $2)"
	fi

	echo "Checking ${dir}"

	attrs=("duration_guideline" \
		"duration_guideline_policy" \
		"max_active_time" \
		"max_active_time_policy" \
		"max_inactive_time" \
		"max_inactive_time_policy")
	for attr in "${attrs[@]}"; do
		if [ ! -f "${dir}/${attr}" ]; then
			exit_failed " --> FAILED (missing ${attr} for ${rw} descriptor $2)"
		fi
	done
}

# Check base directory and its content
sysfsdir="/sys/block/${bdev}/device/duration_limits"

echo "Checking ${sysfsdir}"

if [ ! -d "${sysfsdir}" ]; then
	exit_failed " --> FAILED (no duration_limits sysfs device directory)"
fi

if [ ! -f "${sysfsdir}/enable" ]; then
	exit_failed " --> FAILED (missing enable attribute)"
fi

if [ ! -f "${sysfsdir}/perf_vs_duration_guideline" ]; then
	exit_failed " --> FAILED (missing perf_vs_duration_guideline attribute)"
fi

for rw in read write; do
	# Check descriptor pages and their descriptors
	rwdir="${sysfsdir}/${rw}"
	check_rw "${rwdir}"
	for desc in 1 2 3 4 5 6 7; do
		check_desc "${rwdir}" "${desc}"
	done
done

exit 0
