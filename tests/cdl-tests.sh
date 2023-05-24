#!/bin/bash
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Copyright (C) 2022 Western Digital Corporation or its affiliates.
#

#
# Run everything from the current directory
#
basedir="$(pwd)"
testdir="$(cd "$(dirname "$0")" && pwd)"
export scriptdir="${testdir}/scripts"

. "${scriptdir}/test_lib"

#
# trap ctrl-c interruptions
#
aborted=0
trap ctrl_c INT

function ctrl_c() {
	aborted=1
}

function usage()
{
	echo "Usage: $(basename "$0") [Options] <block device file>"
	echo "Options:"
	echo "  --help | -h             : This help message"
	echo "  --list | -l             : List all tests"
	echo "  --logdir | -g <log dir> : Use this directory to store test log files."
	echo "                            default: cdl-tests-logs/<bdev name>"
	echo "  --test | -t <test num>  : Execute only the specified test case. Can be"
	echo "                            specified multiple times."
	echo "  --force | -f            : Run all tests, even the ones skipped due to"
	echo "                            an inadequate device fw being detected."
	echo "  --quick | -q            : Run quick tests with shorter fio runs."
	echo "                            This can result in less reliable test results."
}

#
# Check existence of required programs
#
require_program "cdladm"
require_program "fio"
require_program "bc"
require_program "time"
require_program "sed"

#
# Parse command line
#
declare -a tests
declare list=false
logdir=""
force_tests=0
quick_tests=0

while [ "${1#-}" != "$1" ]; do
	case "$1" in
	-h | --help)
		usage
		exit 0
		;;
	-t | --test)
		t="${scriptdir}/$2.sh"
		if [ ! -e "$t" ]; then
			echo "Invalid test number $2"
			exit 1;
		fi
		tests+=("$t")
		shift
		shift
		;;
	-l | --list)
		list=true
		shift
		;;
	-g | --logdir)
		shift
		logdir="$1"
		shift
		;;
	-f | --force)
		shift
		force_tests=1
		;;
	-q | --quick)
		shift
		quick_tests=1
		;;
	-*)
		echo "unknown option $1"
		exit 1
		;;
	esac
done

if [ ! $list ] && [ $# -lt 1 ]; then
    usage
    exit 1
fi

#
# Get list of tests
#
if [ "${#tests[@]}" = 0 ]; then
	for f in  ${scriptdir}/*.sh; do
		tests+=("$f")
	done
fi

#
# Handle -l option (list tests)
#
if $list; then
	for t in "${tests[@]}"; do
		echo "  Test $(test_num "$t"): $( $t )"
	done
	exit 0
fi

dev="$1"
if [ -z "$dev" ]; then
	usage
	exit 1
fi

realdev=$(realpath "$dev")
if [ ! -b "$realdev" ]; then
	echo "${dev} is not a block device"
	exit 1
fi

#
# Check credentials
#
if [ $(id -u) -ne 0 ]; then
	echo "Root credentials are required to run tests."
	exit 1
fi

#
# Check target device: when the target is a partition device,
# get basename of its holder device to access sysfs path of
# the holder device
#
bdev=$(basename "$realdev")
major=$((0x$(stat -L -c '%t' "$realdev")))
minor=$((0x$(stat -L -c '%T' "$realdev")))

if [[ -r "/sys/dev/block/$major:$minor/partition" ]]; then
	realsysfs=$(readlink "/sys/dev/block/$major:$minor")
	bdev=$(basename "${realsysfs%/*}")
fi

targetdev="/dev/${bdev}"
if [ "$(dev_cdl_supported ${targetdev})" != "1" ]; then
	echo "${realdev} does not support command duration limits."
	exit 1
fi

if [ "$(kernel_cdl_supported ${targetdev})" != "1" ]; then
	echo "WARNING: System kernel does not support command duration limits."
fi

#
# Prepare log file
#
if [ "${logdir}" == "" ]; then
	logdir="cdl-tests-logs/${bdev}"
fi
rm -rf "${logdir}" > /dev/null 2>&1
mkdir -p "${logdir}"
export logdir

passed=0
total=0
rc=0

#
# Save current IO scheduler
#
saved_sched=$(dev_scheduler "${targetdev}")

#
# Set IO scheduler:
# none for CMR drives, mq-deadline for SMR drives
#
desired_sched=$(get_desired_scheduler "${targetdev}")
set_scheduler "${targetdev}" "${desired_sched}"
if [ $? != 0 ]; then
	echo "$? Set block device scheduler failed."
	exit 1
fi

#
# Save current device queue depth, if ATA
#
if dev_is_ata "${targetdev}"; then
	saved_qd=$(dev_qd "${targetdev}")
fi

export force_tests
export quick_tests

function kmsg_log()
{
	if [ -e /dev/kmsg ]; then
		echo "$1" > /dev/kmsg
	fi
}

function kmsg_log_start()
{
	kmsg_log "#### cdl-tests case $1 start ####"
}

function kmsg_log_end()
{
	kmsg_log "#### cdl-tests case $1 end ####"
}

function parse_dmesg()
{
	local tnum="$1"
	local logdir="$2"
	local start_tag="#### cdl-tests case ${tnum} start ####"
	local end_tag="#### cdl-tests case ${tnum} end ####"
	local test_case_dmesg
	local val

	test_case_dmesg=$(dmesg | tac | \
		sed -e "/${end_tag}/,/${start_tag}/!d" -e "/${start_tag}/q" | tac)
	val=$(echo "$test_case_dmesg" | grep -c "hard resetting link")

	echo "$test_case_dmesg" > "${logdir}/${tnum}_dmesg.log"

	if [ "${val}" -gt 0 ]; then
		echo " --> failed: detected hard reset in dmesg"
		return 1
	fi

	return 0
}

function finalize_log()
{
	local tnum="$1"
	local logdir="$2"
	local log="${logdir}/${tnum}.log"

	# Include fio full log if we have one
	if [ -f "${logdir}/${tnum}_fio.log" ]; then
		echo "" >> "${log}"
		echo "fio full log:" >> "${log}"
		echo "-------------" >> "${log}"
		echo "" >> "${log}"
		cat "${logdir}/${tnum}_fio.log" >> "${log}"
		rm -f "${logdir}/${tnum}_fio.log" > /dev/null 2>&1
	fi

	# Include kernel messages
	if [ -f "${logdir}/${tnum}_dmesg.log" ]; then
		echo "" >> "${log}"
		echo "Kernel messages:" >> "${log}"
		echo "----------------" >> "${log}"
		echo "" >> "${log}"
		cat "${logdir}/${tnum}_dmesg.log" >> "${log}"
		rm -f "${logdir}/${tnum}_dmesg.log" > /dev/null 2>&1
	fi

	# Include fio latency logs if they are not empty
	lat_log="${logdir}/${tnum}_lat.log_clat.log"
	if [ -f "${lat_log}" ]; then
		if [ -s "${lat_log}" ]; then
			echo "" >> "${log}"
			echo "fio completion latency log:" >> "${log}"
			echo "---------------------------" >> "${log}"
			echo "" >> "${log}"
			cat "${lat_log}" >> "${log}"
		fi
		rm -f "${lat_log}" > /dev/null 2>&1
	fi

	lat_log="${logdir}/${tnum}_lat.log_lat.log"
	if [ -f "${lat_log}" ]; then
		if [ -s "${lat_log}" ]; then
			echo "" >> "${log}"
			echo "fio latency log:" >> "${log}"
			echo "----------------" >> "${log}"
			echo "" >> "${log}"
			cat "${lat_log}" >> "${log}"
		fi
		rm -f "${lat_log}" > /dev/null 2>&1
	fi

	lat_log="${logdir}/${tnum}_lat.log_slat.log"
	if [ -f "${lat_log}" ]; then
		if [ -s "${lat_log}" ]; then
			echo "" >> "${log}"
			echo "fio submission latency log:" >> "${log}"
			echo "---------------------------" >> "${log}"
			echo "" >> "${log}"
			cat "${lat_log}" >> "${log}"
		fi
		rm -f "${lat_log}" > /dev/null 2>&1
	fi

	# Remove saved CDL pages
	if [ -f "${logdir}/saved_page.cdl" ]; then
		rm -f "${logdir}/saved_page.cdl" > /dev/null 2>&1
	fi
}

function run_test()
{
	local tnum="$(test_num $1)"
	local dev="$2"
	local logdir="$3"
	local ret=0
	local test_qd

	kmsg_log_start ${tnum}

	echo "==== Test ${tnum}: $( $1 )"
	echo ""

	# Save current device queue depth, if ATA
	if dev_is_ata "${dev}"; then
		test_qd=$(dev_qd "${dev}")
	fi

	"$1" "${dev}"
	ret=$?

	# Restore device queue depth, if ATA
	if dev_is_ata "${dev}"; then
		set_qd "${dev}" "${test_qd}"
	fi

	kmsg_log_end ${tnum}

	parse_dmesg "${tnum}" "${logdir}" || ret=1

	echo ""
	if [ "$ret" == 0 ]; then
		echo "==== Test ${tnum} -> PASS"
	elif [ "$ret" == 2 ]; then
		echo "==== Test ${tnum} -> SKIP"
	elif [ "$ret" == 3 ]; then
		echo "==== Test ${tnum} -> WARNING"
	else
		echo "==== Test ${tnum} -> FAILED"
	fi
	echo ""

	return $ret
}

type="$(devtype ${dev})"
echo "Running CDL tests on ${type} ${dev}:"

cdladm info ${dev} | grep -e Product -e Revision | grep -v SAT

ver="$(cdladm --version | head -1 | cut -f3 -d ' ')"
echo "    Using cdl-tools version ${ver}"

if [ "${force_tests}" == "1" ]; then
	echo -n "    Force all tests: enabled"
else
	echo -n "    Force all tests: disabled"
fi
if [ "${quick_tests}" == "1" ]; then
	echo ", quick tests: enabled"
else
	echo ", quick tests: disabled"
fi

for t in "${tests[@]}"; do
	tnum="$(test_num $t)"

	echo -n "  Test ${tnum}:  "
	printf "%-68s ... " "$( $t )"

	run_test "$t" "$1" "${logdir}" > "${logdir}/${tnum}.log" 2>&1
	ret=$?

	finalize_log "${tnum}" "${logdir}"

	if [ "$ret" == 0 ]; then
		# Test result OK
		status="\e[92mPASS\e[0m"
		rc=0
	elif [ "$ret" == 2 ]; then
		# Test was not applicable
		status="SKIP"
		rc=0
	elif [ "$ret" == 3 ]; then
		# Test passed but warning issued
		status="\e[93mPASS\e[0m"
		rc=0
	else
		# Test failed
		status="\e[31mFAIL\e[0m"
		rc=1
	fi

	if [ "$rc" == 0 ]; then
		((passed++))
	fi
	((total++))
	echo -e "$status"

	if [ "$aborted" == 1 ]; then
		break
	fi

	na=0
done

#
# Restore device queue depth, if ATA
#
if dev_is_ata "${targetdev}"; then
	set_qd "${targetdev}" "${saved_qd}"
fi

#
# Restore IO scheduler
#
set_scheduler "${targetdev}" "${saved_sched}"

echo ""
echo "$passed / $total tests passed"

rm -f local-* >> /dev/null 2>&1

if [ "$passed" != "$total" ]; then
	exit 1
fi

exit 0
