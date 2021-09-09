#!/bin/bash

function usage()
{
	local cmd="$(basename $0)"

	echo "Usage: ${cmd} [Options]"
	echo "Options:"
	echo "  -h | --help    : Print this help message"
	echo "  --all          : Run baseline (no CDL) and CDL workloads (default)"
	echo "  --cdl          : Run only CDL workloads"
	echo "  --dev <disk>   : Specify the target disk"
	echo "  --outdir <dir> : Save results (fio log files) in <dir>"
	echo "                   (default: <disk>_data"
}

# Parse command line
if [ $# -le 1 ]; then
	usage "$0"
	exit 1
fi

outdir=""
dev=""
all=1

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
basedir="$(pwd)"

if [ "${outdir}" == "" ]; then
	outdir="${bdev}_data"
fi

mkdir -p "${outdir}"

function run()
{
	local subdir="$1"
	local fioopts

	shift

	for qd in 1 2 4 8 16 24 32 48 64; do

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

		echo "fio ${fioopts} $@" > fio.log 2>&1
		echo "" >> fio.log 2>&1
		fio ${fioopts} $@ >> fio.log 2>&1

		cd "${basedir}"
	done
}

if [ ${all} == 1 ]; then
	# Run with CDL disabled
	echo "Running baseline workloads (no CDL) on ${dev}..."

	echo 0 > /sys/block/${bdev}/device/duration_limits/enable
	sync

	fioopts=" --ramp_time=30 --runtime=300"

	run "nocdl" ${fioopts}
fi

# Run with CDL enabled
echo "Running CDL workloads on ${dev}..."

echo 1 > /sys/block/${bdev}/device/duration_limits/enable
sync

fioopts=" --ramp_time=120 --runtime=300"
fioopts+=" --cmdprio_percentage=20"
fioopts+=" --cmdprio_class=4 --cmdprio=1"

run "cdl" ${fioopts}

# Disable CDL
echo 0 > /sys/block/${bdev}/device/duration_limits/enable

