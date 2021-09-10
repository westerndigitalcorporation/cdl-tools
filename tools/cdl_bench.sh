#!/bin/bash

basedir="$(pwd)"

function usage()
{
	local cmd="$(basename $0)"

	echo "Usage: ${cmd} [Options]"
	echo "Options:"
	echo "  -h | --help      : Print this help message"
	echo "  --all            : Run baseline (no CDL) and CDL workloads (default)"
	echo "  --cdl            : Run only CDL workloads"
	echo "  --dev <disk>     : Specify the target disk"
	echo "  --ramptime <sec> : Specify the ramp time (seconds) for each run (default: 60)"
	echo "  --runtime <sec>  : Specify the run time (seconds) for each run (default: 300)"
	echo "  --percentage <p> : For cdl run, specify the percentage of commands with cdl"
	echo "  --dld <num>      : For cdl run, specify the descriptor to use"
	echo "  --dldsplit <str> : For cdl run, specify the descriptors to use with their"
	echo "                     percentages, e.g. \"1/33:2/67\". The percentages applies"
	echo "                     to the <percentage> of IOs that will have a high priority."
	echo "  --outdir <dir>   : Save results (fio log files) in <dir>"
	echo "                     (default: cdl_<dld limit>_<percentage>)"
	echo "  --qds <list>     : Specify the list of queue depth to use as a string,"
	echo "                     e.g. \"1 2 4 8 16 32\" (default: \"1 2 4 8 16 24 32\")"
}

# Parse command line
if [ $# -le 1 ]; then
	usage "$0"
	exit 1
fi

dev=""
all=1
ramptime=60
runtime=300
perc=0
dld=0
dldsplit=""
outdir=""
qds=(1 2 4 8 16 24 32)

while [[ $# -gt 0 ]]; do
	case "$1" in
	-h | --help)
		usage "$0"
		exit 0
		;;
	--all)
		all=1
		;;
	--cdl)
		all=0
		;;
	--dev)
		dev="$2"
		shift
		;;
	--ramptime)
		ramptime="$2"
		shift
		;;
	--runtime)
		runtime="$2"
		shift
		;;
	--percentage)
		perc="$2"
		shift
		;;
	--dld)
		dld="$2"
		shift
		;;
	--dldsplit)
		dldsplit="$2"
		dld=0
		shift
		;;
	--qds)
		qds=($2)
		shift
		;;
	--outdir)
		outdir="$2"
		shift
		;;
	-*)
		echo "unknow option $1"
		exit 1
		;;
	esac
	shift
done

if [ "${dev}" == "" ]; then
	echo "No device specified"
	exit 1
fi

bdev="$(basename $(realpath ${dev}))"

if [ ${perc} -eq 0 ]; then
	echo "No CDL percentage specified"
	exit 1
fi

if [ ${perc} -lt 1 ] || [ ${perc} -gt 100 ]; then
	echo "Invalid CDL percentage"
	exit 1
fi

if [ ${dld} -eq 0 ] && [ "${dldsplit}" == "" ]; then
	echo "No CDL descriptor specified"
	exit 1
fi

if [ "${dldsplit}" == "" ]; then
	if [ ${dld} -lt 1 ] || [ ${dld} -gt 7 ]; then
		echo "Invalid CDL descriptor"
		exit 1
	fi
	limit=$(cat /sys/block/${bdev}/device/duration_limits/read/${dld}/duration_guideline)
	limit=$(( limit / 1000 ))
	if [ "${outdir}" == "" ]; then
		outdir="cdl_${limit}_${perc}"
	fi
else
	if [ "${outdir}" == "" ]; then
		outdir="cdl_multi_dld"
	fi
fi

mkdir -p "${outdir}"

function fiorun()
{
	local subdir="$1"

	for qd in ${qds[*]}; do

		rundir="${outdir}/${subdir}/${qd}"
		mkdir -p "${rundir}"
		cd "${rundir}"

		echo "QD=${qd}..."

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
		fioopts+=" --bs=128k"
		fioopts+=" --ioengine=libaio"
		fioopts+=" --iodepth=${qd}"
		fioopts+=" --direct=1"
		if [ ${ramptime} -ne 0 ]; then
			fioopts+=" --ramp_time=${ramptime}"
		fi
		fioopts+=" --runtime=${runtime}"

		if [ "${subdir}" == "cdl" ]; then
			fioopts+=" --cmdprio_percentage=${perc}"
			fioopts+=" --cmdprio_class=4"
			if [ "${dldsplit}" != "" ]; then
				fioopts+=" --cmdprio_split=${dldsplit}"
			else
				fioopts+=" --cmdprio=${dld}"
			fi
		fi

		echo "fio ${fioopts}" > fio.log 2>&1
		echo "" >> fio.log 2>&1
		fio ${fioopts} >> fio.log 2>&1

		cd "${basedir}"
	done
}

echo "Running on ${dev}, ramp time: ${ramptime}s, run time: ${runtime}s"

if [ ${all} == 1 ]; then
	# Run with CDL disabled
	echo "Running baseline (no CDL)..."

	echo 0 > /sys/block/${bdev}/device/duration_limits/enable
	sync

	fiorun "nocdl"
fi

# Run with CDL enabled
echo "Running CDL..."

echo 1 > /sys/block/${bdev}/device/duration_limits/enable
sync

subdir="cdl"
rundir="${outdir}/${subdir}"
mkdir -p "${rundir}"
grep . /sys/block/${bdev}/device/duration_limits/read/*/duration_guideline > "${rundir}/limits"

fiorun "${subdir}"

# Disable CDL
echo 0 > /sys/block/${bdev}/device/duration_limits/enable

