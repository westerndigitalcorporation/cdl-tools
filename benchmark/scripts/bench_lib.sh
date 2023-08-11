#!/bin/sh

function exit_failed()
{
	echo "$1"
	exit 1
}

function require_program()
{
	[[ $(type -P "$1") ]] || \
		{
			echo "$1 not found."
			echo "Installing $1 is required."
			exit 1
		}
}

function require_fio_cmdprio_hint()
{
	require_program "fio"

	# Check that the version of fio installed has the cmdprio_hint option
	fio --name="test" --filename=/dev/null \
		--ioengine=io_uring --cmdprio_hint=2 --size=1M > /dev/null 2>&1
	if [ $? -eq 1 ]; then
		echo "The installed version of fio does not seem to have CDL support"
		echo "(--cmdprio_hint option is not supported)"
		exit 1
	fi
}

function ncqprio_supported()
{
	local dev="$1"
	local bdev="$(basename $(realpath ${dev}))"
	local supported="/sys/block/${bdev}/device/ncq_prio_supported"

	if [ -f "${supported}" ]; then
		cat "${supported}"
	else
		echo "0"
	fi
}

function ncqprio_enable()
{
	local dev="$1"
        local onoff="$2"
	local bdev="$(basename $(realpath ${dev}))"

	if [ ${onoff} -eq 1 ]; then
		# Enable priority mode
		sg_sat_set_features --feature=0x57 "${dev}"

		# Set knob (count between 20 and 200 == -2% to -20% IOPS loss)
		sg_sat_set_features --feature=0x58 --count=200 "${dev}"

		# Enable IO priority propagation to the device
		echo 1 > /sys/block/${bdev}/device/ncq_prio_enable
	else
		# Enable IO priority propagation to the device
		echo 0 > /sys/block/${bdev}/device/ncq_prio_enable

		# Disable priority mode
		sg_sat_set_features --feature=0x56 "${dev}"
	fi
	sync
}

function cdl_supported()
{
	local dev="$1"
	local bdev="$(basename $(realpath ${dev}))"
	local supported="/sys/block/${bdev}/device/cdl_supported"

	if [ -f "${supported}" ]; then
		cat "${supported}"
	else
		echo "0"
	fi
}

function set_cdl_pages()
{
	local dev="$1"
	local t2a="$2"
	local t2b="$3"

	echo "Uploading T2A CDL page"
	cdladm upload --file "${t2a}" "${dev}" > /dev/null 2>&1 || \
		exit_failed "Load T2A CDL page failed"

	echo "Uploading T2B CDL page"
	cdladm upload --file "${t2b}" "${dev}" > /dev/null 2>&1 || \
		exit_failed "Load T2B CDL page failed"
}

function cdl_enable()
{
	local dev="$1"
	local onoff="$2"
	local bdev="$(basename $(realpath ${dev}))"

	echo "${onoff}" > /sys/block/${bdev}/device/cdl_enable
	sync
}

#
# $1: fio lat log file
#
function get_prios()
{
	local fiolatlog="$1"

	# The priority field is hex (starts with "0x") and
	# is the last field on each line
	echo $(awk '/0x/{print $NF}' "${fiolatlog}" | sort -u)
}

#
# $1: fio lat log file
#
function get_high_prios()
{
	local fiolatlog="$1"

	# The priority field is hex (starts with "0x") and
	# is the last field on each line
	echo $(awk '/0x/{print $NF}' "${fiolatlog}" | sort -u | grep -v "0x0000")
}

#
# $1: a prio value (0x....)
#
function get_prio_class()
{
	echo $(( $1 >> 13 ))
}

#
# $1: a prio value (0x....)
#
function get_prio_hint()
{
	echo $(( ($1 >> 3) & 0x3FF ))
}

#
# $1: a prio value (0x....)
#
function get_prio_level()
{
	echo $(( $1 & 0x03 ))
}

#
# $1: a prio value (0x....)
#
function get_prio_class_name()
{
	local class=$(get_prio_class $1)

	case ${class} in
	0)
		echo "NONE";;
	1)
		echo "RT";;
	2)
		echo "BE";;
	3)
		echo "IDLE";;
	*)
		echo "??";;
	esac
}

#
# $1: fio lat log file
# $2: a prio value (0x....)
#
function get_prio_bytes()
{
	local fiolatlog="$1"
	local prio="$2"

	grep "${prio}" "${fiolatlog}" | \
		awk '{sum += $4}END{printf "%d",sum}'
}

#
# $1: fio lat log file
#
function get_runtime()
{
	local fiolatlog="$1"

	# Get run time in milliseconds
        echo "$(tail -1 "${fiolatlog}" | cut -d',' -f1)"
}

#
# $1: Sorted prio lat file
# $2: Duration in milliseconds
#
function get_prio_iops()
{
	local priolat="$1"
	local duration="$2"
	local nio

	nio=$(wc -l < "${priolat}")

	echo "scale=1; ${nio}*1000/${duration}" | bc
}

#
# $1: fio lat log file
# $2: a prio value (0x....)
#
function gen_priolat()
{
	local fiolatlog="$1"
	local prio="$2"
	local priolat="$(mktemp)"

	grep "${prio}" "${fiolatlog}" | \
		awk 'BEGIN {FS = ","}; {printf "%d\n",$2/1000000}' | \
		sort -n > "${priolat}"

	echo "${priolat}"
}

#
# $1: Sorted prio lat file
#
function get_lat_min()
{
	local fiolatlog="$1"

	head -1 "${priolat}"
}

#
# $1: Sorted prio lat file
#
function get_lat_max()
{
	local priolat="$1"

	tail -1 "${priolat}"
}

#
# $1: Sorted prio lat file
#
function get_lat_avg()
{
	local priolat="$1"

	awk '{sum += $1}END{printf "%.02f",sum/NR}' "${priolat}"
}

#
# $1: Sorted prio lat file
#
function get_lat_mean()
{
	local priolat="$1"

	# The input file must be sorted
	awk '{all[NR] = $0}END{printf "%d",all[int(NR/2)]}' "${priolat}"
}

#
# $1: Sorted prio lat file
#
function get_lat_stdev()
{
	awk '{x+=$0;y+=$0^2}END{printf "%.02f",sqrt(y/NR-(x/NR)^2)}' "${priolat}"
}

#
# $1: Sorted prio lat file
# $2: Percentile as a float
#
function get_lat_percentile()
{
	local priolat="$1"
	local p="$2"

	# The input file must be sorted
	percentile=$(awk -v p=${p} '{all[NR] = $0}END{printf "%d",all[int(NR*p)]}' "${priolat}")
	if [ ${terse} -eq 1 ]; then
		printf "%d" ${percentile}
	else
		printf "%5d" ${percentile}
	fi
}

#
# dldsplit format: dld1/perc1,dls2/perc2,...
#
function gen_fio_bssplit()
{
	local bs="$1"
	local class="$2"
	local dldsplit="$3"
	local bssplit=""

	OIFS=$IFS
	IFS=','
	for s in ${dldsplit}; do
		dld="$(echo ${s} | cut -d'/' -f1)"
		if [ ${dld} -gt 7 ]; then
			echo "Invalid DLD index specified"
			exit 1
		fi
		perc=$(echo ${s} | cut -d'/' -f2)
		if [ ${perc} -lt 1 ] || [ ${perc} -gt 100 ]; then
			echo "Invalid DLD index percentage specified"
			exit 1
		fi
		if [ "${bssplit}" != "" ]; then
			bssplit+=":"
		fi
		bssplit+="${bs}/${perc}/${class}/0/${dld}"
	done

	IFS=$OIFS

	echo "${bssplit}"
}

function fiorun()
{
	local run="$1"

	rundir="${outdir}/${run}"
	mkdir -p "${rundir}"

	if [ "${run}" == "cdlsingle" ] || [ "${run}" == "cdlmulti" ]; then
		cdladm show "${dev}" > "${rundir}/cdl_descriptors"
	fi

	for qd in ${qds[*]}; do

		rundir="${outdir}/${run}/${qd}"
		mkdir -p "${rundir}"
		cd "${rundir}"

		echo "  QD=${qd}..."

		fioopts="--name=rndrd_qd${qd}"
		fioopts+=" --filename=${dev}"
		fioopts+=" --random_generator=tausworthe64"
		fioopts+=" --continue_on_error=none"
		fioopts+=" --group_reporting=1"
		fioopts+=" --ioscheduler=none"
		fioopts+=" --write_lat_log=randread.log"
		fioopts+=" --per_job_logs=0"
		fioopts+=" --log_prio=1"
		fioopts+=" --numjobs=1"
		fioopts+=" --rw=randread"
		fioopts+=" --bs=${bs}"
		fioopts+=" --ioengine=libaio"
		fioopts+=" --iodepth=${qd}"
		fioopts+=" --direct=1"
		if [ ${ramptime} -ne 0 ]; then
			fioopts+=" --ramp_time=${ramptime}"
		fi
		fioopts+=" --runtime=${runtime}"

		if [ "${run}" == "ncqprio" ]; then
			fioopts+=" --cmdprio_percentage=${perc}"
			fioopts+=" --cmdprio_class=1"
		elif [ "${run}" == "cdlsingle" ]; then
			fioopts+=" --cmdprio_percentage=${perc}"
			fioopts+=" --cmdprio_class=2"
			fioopts+=" --cmdprio_hint=${dld}"
		elif [ "${run}" == "cdlmulti" ]; then
			fioopts+=" --cmdprio_bssplit="
			fioopts+="$(gen_fio_bssplit ${bs} 2 ${dldsplit})"
		fi

		echo "fio ${fioopts}" > fio.log 2>&1
		echo "" >> fio.log 2>&1
		fio ${fioopts} >> fio.log 2>&1
	done
}
