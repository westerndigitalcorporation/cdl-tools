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
	echo "  --test | -t <test num>  : Execute only the specified test case. This"
	echo "                            option can be specified multiple times."
	echo "  --group | -g <group num>: Execute only the tests belonging to the"
	echo "                            specified test group. This option can be"
	echo "                            specified multiple times."
	echo "  --repeat | -r <num>     : Repeat the execution of the selected test cases"
	echo "                            <num> times (default: tests are executed once)."
	echo "  --run-time <seconds>    : Specify fio run time in seconds. Short run"
	echo "                            times can result in less reliable test results."
	echo "                            Default: 60 seconds"
	echo "  --stop-on-error         : For test cases that expect an error, tell"
	echo "                            fio to stop immediately when an IO error"
	echo "                            is detected. Default: continue running"
	echo "  --quick | -q            : Same as \"--run-time 20 --stop-on-error\""
	echo "  --logdir <log dir>      : Use this directory to store test log files."
	echo "                            default: logs/<bdev name>"
}

#
# Check existence of required programs
#
require_program "cdladm"
require_fio_cmdprio_hint
require_program "bc"
require_program "time"
require_program "sed"

#
# Parse command line
#
declare -a groups
declare -a tests
declare list=false
logdir=""
repeat=1

export fio_run_time="60"
export fio_stop_on_error="0"

while [ "${1#-}" != "$1" ]; do
	case "$1" in
	-h | --help)
		usage
		exit 0
		;;
	-t | --test)
		t="$(test_file_from_num "$2")"
		if [ ! -e "$t" ]; then
			echo "Invalid test number $2"
			exit 1;
		fi
		if [[ ! " ${tests[*]} " =~ " ${t} " ]]; then
			tests+=("$t")
		fi
		shift
		shift
		;;
	-g | --group)
		if (( $2 < 0 )) || (( $2 >= $(nr_groups) )); then
			echo "Invalid group number $2"
			exit 1;
		fi
		if [[ ! " ${groups[*]} " =~ " $2 " ]]; then
			groups+=("$2")
		fi
		shift
		shift
		;;
	-l | --list)
		list=true
		shift
		;;
	--run-time)
		shift
		fio_run_time="$1"
		shift
		;;
	-q | --quick)
		fio_run_time="20"
		fio_stop_on_error="1"
		shift
		;;
	-r | --repeat)
		shift
		repeat="$1"
		shift
		;;
	--logdir)
		shift
		logdir="$1"
		shift
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

# If no group was specified, and -t option was not used, add all groups
if [ "${#groups[@]}" = 0 ] && [ "${#tests[@]}" = 0 ]; then
	for gdir in ${scriptdir}/0?_*; do
		groups+=("$(group_num_from_dir ${gdir})")
	done
fi

# Add test cases from the selected groups to the test list
for g in "${groups[@]}"; do
	gdir="$(group_dir_from_num ${g})"
	if [ ! -d "${gdir}" ]; then
		echo "Unknown group \"${g}\""
		exit 1
	fi

	for t in ${gdir}/*.sh; do
		if [[ ! " ${tests[*]} " =~ " ${t} " ]]; then
			tests+=("$t")
		fi
	done
done

#
# Handle -l option (list tests)
#
if $list; then
	gnum="XX"

	for t in "${tests[@]}"; do
		tnum="$(test_num "$t")"
		gn="$(test_group_num "${tnum}")"
		if [ "${gn}" != "${gnum}" ]; then
			echo "Group ${gn}: $(group_name ${gn}) tests"
			gnum="${gn}"
		fi
		echo "  Test ${tnum}: $( $t )"
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
	echo "System kernel does not support command duration limits."
	exit 1
fi

#
# Prepare log file
#
if [ "${logdir}" == "" ]; then
	logdir="logs/${bdev}"
	rm -rf "${logdir}" > /dev/null 2>&1
fi
mkdir -p "${logdir}"
export logdir

runlog="${logdir}/cdl-tests.log"

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

# Start logging the run
{

type="$(devtype ${dev})"
echo "Running CDL tests on ${type} ${dev}:"

cdladm info ${dev} | grep -e Product -e Revision | grep -v SAT

ver="$(cdladm --version | head -1 | cut -f3 -d ' ')"
echo "    Using cdl-tools version ${ver}"

echo -n "fio: ${fio_run_time}s run time, stop on error "
if [ "${fio_stop_on_error}" == "1" ]; then
	echo "enabled"
else
	echo "disabled"
fi
echo ""

for ((iter=1; iter<=repeat; iter++)); do

	if [ ${repeat} -ne 1 ]; then
		echo "Iteration ${iter} / ${repeat}:"
		ldir="${logdir}/run-${iter}"
		mkdir -p "${ldir}"
	else
		ldir="${logdir}"
	fi

	gnum="XX"

	for t in "${tests[@]}"; do
		tnum="$(test_num $t)"

		gn="$(test_group_num "${tnum}")"
		if [ "${gn}" != "${gnum}" ]; then
			echo "Group ${gn}: $(group_name ${gn})"
			gnum="${gn}"
		fi

		echo -n "  Test ${tnum}:  "
		printf "%-68s ... " "$( $t )"

		run_test "$t" "$1" "${ldir}" > \
			"${ldir}/${tnum}.log" 2>&1
		ret=$?

		finalize_log "${tnum}" "${ldir}"

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

	if [ "$aborted" == 1 ]; then
		break
	fi
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

# End logging the run
} | tee -i "${runlog}" 2>&1

rm -f local-* >> /dev/null 2>&1
unset logdir
unset scriptdir
unset fio_run_time
unset fio_stop_on_error

if [ "$passed" != "$total" ]; then
	exit 1
fi

exit 0
