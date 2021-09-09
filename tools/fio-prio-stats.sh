#!/bin/sh

basedir="$(cd "$(dirname "$0")" && pwd)"
scriptdir="${basedir}/scripts"

. "${scriptdir}/cdl_lib.sh"

cmd="$(basename $0)"

# Default option values
terse=0
tersehead=""
savepriolat=0

function usage()
{
	echo "Usage: ${cmd} [Options] <fio lat log file>"
	echo "Options:"
	echo "  -h | --help    : Print this help message"
	echo "  --terse        : Output stats in csv format. The fields are in order:"
	echo "                   priority class, priority level, iops, bw (MB/s), "
	echo "                   latency min, max and average, and latency percentiles"
	echo "                   (1.00th, 5.00th, 10.00th, 20.00th, 30.00th, 40.00th,"
	echo "                    50.00th, 60.00th, 70.00th, 80.00th, 90.00th, 95.00th, "
	echo "                    99.00th, 99.50th, 99.90th, 99.95th and 99.99th)"
	echo "  --head <str>   : Add <str> as the first field of the terse output"
	echo "  --save-priolat : Save to different files the sorted latencies of I/Os"
	echo "                   with the same priority. The files are name"
	echo "                   priolat.<class name>.<level>.log, with class name"
	echo "                   being NONE, RT, BE, IDLE or CDL"
}

# Pares the command line
while [ "${1#-}" != "$1" ]; do

	case "$1" in
	-h | --help)
		usage
		exit 0
		;;
	--terse)
		terse=1
		;;
	--head)
		tersehead="$2"
		shift
		;;
	--save-priolat)
		savepriolat=1
		;;
	-*)
		echo "unknow option $1"
		exit 1
		;;
	esac
	shift
done

if [ $# != 1 ]; then
	usage
	exit 1
fi

fiolatlog="$1"

percentiles=(0.01 0.05 \
	0.10 0.20 0.30 0.40 0.50 0.60 0.70 0.80 0.90 \
	0.95 0.99 0.995 0.999 0.9995 0.9999)

function get_prio_stats()
{
	local priolat="$1"
	local prio="$2"
	declare -a p

	nio=$(cat ${fiolatlog} | wc -l)
	prionio=$(cat ${priolat} | wc -l)
	perc=$(echo "scale=2; ${prionio} * 100 / ${nio}" | bc)

	class=$(get_prio_class_name ${prio})
	level=$(get_prio_level ${prio})
	echo "Priority ${prio}, class ${class}, level ${level} (${perc} %):"

	runtime=$(get_runtime "${fiolatlog}")
	iops=$(get_prio_iops "${priolat}" "${runtime}")

	bytes=$(get_prio_bytes "${fiolatlog}" "${prio}")
	mib=$(( bytes / 1048576 ))
	bw=$(echo "scale=1; ${mib} * 1000 / ${runtime}" | bc)
	bw2=$(echo "scale=1; ${bytes} / (${runtime} * 1000)" | bc)
	echo "    IOPS=${iops}, BW=${bw}MiB/s (${bw2}MB/s)(${mib}MiB/${runtime}msec)"

	lat_min=$(get_lat_min "${priolat}")
	lat_max=$(get_lat_max "${priolat}")
	lat_avg=$(get_lat_avg "${priolat}")
	lat_stdev=$(get_lat_stdev "${priolat}")
	echo "    lat (msec): min=${lat_min}, max=${lat_max}, avg=${lat_avg}, stdev=${lat_stdev}"

	for i in "${!percentiles[@]}"; do
		p[$i]=$(get_lat_percentile "${priolat}" "${percentiles[$i]}")
	done

	echo "    lat percentiles (msec):"
	echo "     |  1.00th=[${p[0]}],  5.00th=[${p[1]}], 10.00th=[${p[2]}], 20.00th=[${p[3]}],"
	echo "     | 30.00th=[${p[4]}], 40.00th=[${p[5]}], 50.00th=[${p[6]}], 60.00th=[${p[7]}],"
	echo "     | 70.00th=[${p[8]}], 80.00th=[${p[9]}], 90.00th=[${p[10]}], 95.00th=[${p[11]}],"
	echo "     | 99.00th=[${p[12]}], 99.50th=[${p[13]}], 99.90th=[${p[14]}], 99.95th=[${p[15]}],"
	echo "     | 99.99th=[${p[16]}]"
}

function get_prio_stats_terse()
{
	local priolat="$1"
	local prio="$2"
	declare -a p

	class=$(get_prio_class_name ${prio})
	level=$(get_prio_level ${prio})

	runtime=$(get_runtime "${fiolatlog}")
	iops=$(get_prio_iops "${priolat}" "${runtime}")

	bytes=$(get_prio_bytes "${fiolatlog}" "${prio}")
	bw=$(echo "scale=1; ${bytes} / (${runtime} * 1000)" | bc)
	lat_min=$(get_lat_min "${priolat}")
	lat_max=$(get_lat_max "${priolat}")
	lat_avg=$(get_lat_avg "${priolat}")

	if [ "${tersehead}" != "" ]; then
		echo -n "${tersehead},"
	fi
	echo -n "${class},${level},${iops},${bw},${lat_min},${lat_max},${lat_avg}"

	for i in "${!percentiles[@]}"; do
		p[$i]=$(get_lat_percentile "${priolat}" "${percentiles[$i]}")
		echo -n ",${p[i]}"
	done
	echo ""
}

# Get the list of priorities in the fio log file
prios=($(get_prios "${fiolatlog}"))
if [ ${#prios[@]} -eq 0 ]; then
	echo "Log file does not have any priority information"
	exit 1
fi

# Extract the latency log of each priority
for prio in ${prios[*]}; do

	# Extract the IO logs for the current priority
	priolat=$(gen_priolat "${fiolatlog}" "${prio}")

	# Get statictics
	if [ ${terse} -eq 1 ]; then
		get_prio_stats_terse "${priolat}" "${prio}"
	else
		get_prio_stats "${priolat}" "${prio}"
	fi
	
	if [ ${savepriolat} -eq 1 ]; then
		class=$(get_prio_class_name ${prio})
		level=$(get_prio_level ${prio})
		dir="$(cd "$(dirname "${fiolatlog}")" && pwd)"
		mv "${priolat}" "${dir}/priolat.${class}.${level}.log"
	else
		rm -f "${priolat}"
	fi
done

