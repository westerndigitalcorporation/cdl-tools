#!/bin/bash

basedir="$(cd "$(dirname "$0")" && pwd)"
scriptdir="${basedir}/scripts"

. "${scriptdir}/bench_lib.sh"

require_fio_cmdprio_hint

# Defaults
dev=""
baseline=0
ncqprio=0
cdlsingle=0
cdlmulti=0

bs="$(( 128 * 1024 ))"
ramptime=60
runtime=300
perc=0
dld=0
dldsplit=""
outdir=""
qds=(1 2 4 8 16 24 32)
t2a="${scriptdir}/T2A.cdl"
t2b="${scriptdir}/T2B.cdl"
load_cdl_pages=0

function usage()
{
	local cmd="$(basename $0)"

	echo "Usage: ${cmd} [Options]"
	echo "Options:"
	echo "  -h | --help      : Print this help message"
	echo "  --dev <file>     : Specify the target device"
	echo "  --bs <size>      : Random read IO size (default: ${bs})"
	echo "  --ramptime <sec> : Specify the ramp time (seconds) for each run"
	echo "                     (default: ${ramptime})"
	echo "  --runtime <sec>  : Specify the run time (seconds) for each run"
	echo "                     (default: ${runtime})"
	echo "  --qds <list>     : Specify the list of queue depths to use,"
	echo "                     E.g. \"1 2 4 8 16 32\" (default: \"1 2 4 8 16 24 32\")"
	echo "  --outdir <dir>   : Save the run results in <dir>. <dir> must not exist."
	echo "                     (default: ${HOME}/<dev name>_cdl_bench)"

	echo "  --baseline       : Run baseline workload"
	echo "  --ncq-prio       : Use NCQ priority workload"
	echo "  --cdl-single     : Run CDL workload with a single limit"
	echo "  --cdl-multi      : Run CDL workload with multiple limits"

	echo "  --percentage <p> : For ncq-prio and cdl-single runs, specify the"
	echo "                     percentage of commands with a high-priority/limit"
	echo "  --dld <index>    : For a cdl-single run, specify the descriptor index"
	echo "                     to use"
	echo "  --dldsplit <str> : For a cdl-multi run, comma separated list of the"
	echo "                     CDL descriptors to use with the percentage of I/Os"
	echo "                     E.g. \"1/10,2/20\" for 10% of I/Os with CDL 1,"
	echo "                     20% of I/Os with CDL 2 and the remaining I/Os"
	echo "                     (70%) with no limit".
	echo "  --t2a <file>     : For cdl-single and cdl-multi runs, specify the"
	echo "                     T2A CDL descriptor page to use."
	echo "                     (default ${t2a})"
	echo "  --t2b <file>     : For cdl-single and cdl-multi runs, specify the"
	echo "                     T2B CDL descriptor page to use."
	echo "                     (default ${t2b})"
}

# Parse command line
if [ $# -le 1 ]; then
	usage "$0"
	exit 1
fi

while [[ $# -gt 0 ]]; do
	case "$1" in
	-h | --help)
		usage "$0"
		exit 0
		;;

	--dev)
		dev="$2"
		shift
		;;
	--bs)
		bs="$2"
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
	--qds)
		qds=($2)
		shift
		;;
	--outdir)
		outdir="$2"
		shift
		;;

	--baseline)
		baseline=1
		;;
	--ncq-prio)
		ncqprio=1
		;;
	--cdl-single)
		cdlsingle=1
		load_cdl_pages=1
		;;
	--cdl-multi)
		cdlmulti=1
		load_cdl_pages=1
		;;

	--percentage)
		perc="$2"
		if [ ${perc} -lt 1 ] || [ ${perc} -gt 100 ]; then
			echo "Invalid percentage"
			exit 1
		fi
		shift
		;;
	--dld)
		dld="$2"
		if [ ${dld} -lt 1 ] || [ ${dld} -gt 7 ]; then
			echo "Invalid limit index"
			exit 1
		fi
		shift
		;;
	--dldsplit)
		dldsplit="$2"
		shift
		;;
	--t2a)
		t2a="$2"
		shift
		;;
	--t2b)
		t2b="$2"
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

runs=$(( baseline + ncqprio + cdlsingle + cdlmulti ))
if [ ${runs} -eq 0 ]; then
	echo "Nothing to run"
	exit 1
fi

if [ ${ncqprio} -eq 1 ]; then
	if [ "$(ncqprio_supported ${dev})" == "0" ]; then
		echo "${dev} does not support NCQ priority"
		exit 1
	fi

	if [ ${perc} -eq 0 ]; then
		echo "No percentage specified"
		exit 1
	fi
fi

if [ ${cdlsingle} -eq 1 ]; then
	if [ "$(cdl_supported ${dev})" == "0" ]; then
		echo "${dev} does not support CDL"
		exit 1
	fi

	if [ ${perc} -eq 0 ]; then
		echo "No percentage specified"
		exit 1
	fi

	if [ ${dld} -eq 0 ]; then
		echo "No CDL descriptor specified"
		exit 1
	fi
fi

if [ ${cdlmulti} -eq 1 ]; then
	if [ "$(cdl_supported ${dev})" == "0" ]; then
		echo "${dev} does not support CDL"
		exit 1
	fi

	if [ "${dldsplit}" == "" ]; then
		echo "No CDL specified"
		exit 1
	fi
fi

if [ "${outdir}" == "" ]; then
	bdev="$(basename $(realpath ${dev}))"
	outdir="${HOME}/${bdev}_cdl_bench"
fi
[ -d "${outdir}" ] && exit_failed "Output directory ${outdir} exists. Move it out of the way"
mkdir -p "${outdir}" || exit_failed "Create output directory failed"

echo "Run on ${dev}, ramp time: ${ramptime}s, run time: ${runtime}s"
echo "  Output directory: ${outdir}"
echo ""
cdladm info ${dev}

if [ ${load_cdl_pages} -eq 1 ]; then
	ncqprio_enable "${dev}" 0
	set_cdl_pages "${dev}" "${t2a}" "${t2b}"
fi

echo ""

if [ ${baseline} == 1 ]; then
	echo "Running baseline workload"

	cdl_enable "${dev}" 0
	ncqprio_enable "${dev}" 0

	fiorun "baseline"
fi

if [ ${ncqprio} == 1 ]; then
	echo "Running NCQ priority workload"

	cdl_enable "${dev}" 0
	ncqprio_enable "${dev}" 1

	fiorun "ncqprio"
fi

if [ ${cdlsingle} == 1 ]; then
	echo "Running CDL workload, single limit"

	ncqprio_enable "${dev}" 0
	cdl_enable "${dev}" 1

	fiorun "cdlsingle"
fi

if [ ${cdlmulti} == 1 ]; then
	echo "Running CDL workload, multiple limits"

	ncqprio_enable "${dev}" 0
	cdl_enable "${dev}" 1

	fiorun "cdlmulti"
fi

cdl_enable "${dev}" 0
ncqprio_enable "${dev}" 0
