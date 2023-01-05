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
require_lib "libaio"

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

function run_test()
{
	local tnum="$(test_num $1)"
	local ret=0

	kmsg_log_start ${tnum}

	echo "==== Test ${tnum}: $( $1 )"
	echo ""

	"$1" "$2"
	ret=$?

	echo ""
	if [ "$ret" == 0 ]; then
		echo "==== Test ${tnum} -> PASS"
	elif [ "$ret" == 2 ]; then
		echo "==== Test ${tnum} -> skip"
	else
		echo "==== Test ${tnum} -> FAILED"
	fi
	echo ""

	kmsg_log_end ${tnum}

	return $ret
}

type="$(devtype ${dev})"
echo "Running CDL tests on ${type} ${dev}:"
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

	run_test "$t" "$1" > "${logdir}/${tnum}.log" 2>&1
	ret=$?
	if [ "$ret" == 0 ]; then
		# Test result OK
		status="\e[92mPASS\e[0m"
		rc=0
	elif [ "$ret" == 2 ]; then
		# Test was not applicable
		status="skip"
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
