#!/bin/bash

basedir="$(cd "$(dirname "$0")" && pwd)"
scriptdir="${basedir}/scripts"

. "${scriptdir}/cdl_lib.sh"

function usage()
{
	local cmd="$(basename $0)"

	echo "Usage: ${cmd} [Options] <data dir>"
	echo "Options:"
	echo "  -h | --help      : Print this help message"
	echo "  --save           : Save plots to png files instead of displaying them"
	echo "  --cdl-limit <ms> : Specify the duration limit used in milliseconds"
}

# Parse command line
if [ $# -lt 1 ]; then
	usage "$0"
	exit 1
fi

gsave=0
gcmd="gnuplot -p"
cdllimit="??"

while [ "${1#-}" != "$1" ]; do
	case "$1" in
	-h | --help)
		usage "$0"
		exit 0
		;;
	--save)
		gsave=1
		gcmd="gnuplot"
		;;
	--cdl-limit)
		cdllimit="$2"
		shift
		;;
	-*)
		echo "unknow option $1"
		exit 1
		;;
	esac
	shift
done

datadir="$(cd "$1" && pwd)"
basedir="$(pwd)"

function gopts()
{
        local png="$1"
        local opts

        if [ ${gsave} -eq 1 ]; then
		opts="set terminal pngcairo dashed size 860,400;"
        	opts+="set output '${png}';"
        	echo "${opts}"
	else
		echo ""
	fi
}

function plot_nocdl_iops()
{
        local csvf="$1"
        local opts

	echo "Plotting IOPS"

	opts=$(gopts "nocdl_iops.png")
        opts+="filename='${csvf}'"

	${gcmd} -e "${opts}" "${scriptdir}/nocdl_iops.gnuplot" > /dev/null 2>&1
}

function plot_nocdl_lat_avg()
{
        local csvf="$1"
        local opts

	echo "Plotting latency average"

	opts=$(gopts "nocdl_lat_avg.png")
        opts+="filename='${csvf}'"

	${gcmd} -e "${opts}" "${scriptdir}/nocdl_lat_avg.gnuplot" > /dev/null 2>&1
}

function plot_nocdl_lat_p99()
{
        local csvf="$1"
        local opts

	echo "Plotting latency 99th percentile"

        opts=$(gopts "nocdl_lat_p99.png")
        opts+="filename='${csvf}'"

        ${gcmd} -e "${opts}" "${scriptdir}/nocdl_lat_p99.gnuplot" > /dev/null 2>&1
}

function plot_nocdl_lat_hist()
{
	local tmpf="$(mktemp)"
	local latfiles=""

	echo "Plotting latency distribution"

	# Concatenate the sorted IO latency files
	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		latfiles+=" ${qd}/priolat.NONE.0.log"
	done
	paste -d',' ${latfiles} > "${tmpf}"

	opts=$(gopts "nocdl_lat_hist.png")
        opts+="filename='${tmpf}';"
	opts+="ptitle='I/O Latency Probability Distribution'"

	${gcmd} -e "${opts}" "${scriptdir}/lat_hist.gnuplot" > /dev/null 2>&1

	rm -f "${tmpf}"
}

function process_nocdl()
{
	local d="$1"

	echo "Processing baseline (no CDL) results"

	cd "${d}"

	resf="randread.csv"
	rm -rf "${resf}"

	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		echo "  QD=${qd}"
		${scriptdir}/../fio-prio-stats.sh \
			--save-priolat \
			--terse \
			--head "${qd}" \
			${qd}/randread.log_lat.log >> ${resf}
	done

	plot_nocdl_iops ${resf}
	plot_nocdl_lat_avg ${resf}
	plot_nocdl_lat_p99 ${resf}
	plot_nocdl_lat_hist

	cd ..
}

function plot_cdl_iops()
{
	local resfnocdl="$1"
	local resfcdl="$2"
	local iopsnocdl="$(mktemp)"
	local iopscdl="$(mktemp)"
	local iops="$(mktemp)"
	local iopstotal="$(mktemp)"
	local qdf="$(mktemp)"

	echo "Plotting IOPS"

	cat ${resfnocdl} | cut -d',' -f4 > "${iopsnocdl}"
	cat ${resfcdl} | cut -d',' -f4 > "${iopscdl}"
	paste "${iopsnocdl}" "${iopscdl}" > "${iops}"
	awk '{print $1 + $2}' "${iops}" > "${iopstotal}"

	# Concatenate the sorted IO latency files
	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		echo "${qd}" >> "${qdf}"
	done

	paste -d',' "${qdf}" "${iopsnocdl}" "${iopscdl}" "${iopstotal}" > "${iops}"

	opts=$(gopts "cdl_iops.png")
        opts+="filename='${iops}';"
	opts+="limit='${cdllimit} ms'"
	${gcmd} -e "${opts}" "${scriptdir}/cdl_iops.gnuplot" > /dev/null 2>&1

	rm -f "${qdf}"
	rm -f "${iops}"
	rm -f "${iopscdl}"
	rm -f "${iopsnocdl}"
	rm -f "${iopstotal}"
}

function plot_cdl_lat_avg()
{
	local resfnocdl="$1"
	local resfcdl="$2"

	echo "Plotting latency average"

	opts=$(gopts "cdl_lat_avg.png")
        opts+="f1='${resfnocdl}';"
        opts+="f2='${resfcdl}';"
	opts+="limit='${cdllimit} ms'"

	${gcmd} -e "${opts}" "${scriptdir}/cdl_lat_avg.gnuplot" > /dev/null 2>&1
}

function plot_cdl_lat_p99()
{
	local resfnocdl="$1"
	local resfcdl="$2"

	echo "Plotting latency 99th percentile"

	opts=$(gopts "cdl_lat_p99.png")
	opts+="f1='${resfnocdl}';"
	opts+="f2='${resfcdl}';"
	opts+="limit='${cdllimit} ms'"

	${gcmd} -e "${opts}" "${scriptdir}/cdl_lat_p99.gnuplot" > /dev/null 2>&1
}

function plot_cdl_lat_hist()
{
	local lathistnocdl="$(mktemp)"
	local latfilesnocdl=""
	local lathistcdl="$(mktemp)"
	local latfilescdl=""

	# Concatenate the sorted IO latency files
	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		latfilesnocdl+=" ${qd}/priolat.NONE.0.log"
		latfilescdl+=" ${qd}/priolat.CDL.1.log"
	done
	paste -d',' ${latfilesnocdl} > "${lathistnocdl}"
	paste -d',' ${latfilescdl} > "${lathistcdl}"

	opts=$(gopts "cdl_lat_hist_nolimit.png")
	opts+="filename='${lathistnocdl}';"
	opts+="ptitle='I/O Latency Distribution, No Limit I/Os'"

	${gcmd} -e "${opts}'" "${scriptdir}/lat_hist.gnuplot" > /dev/null 2>&1

	opts=$(gopts "cdl_lat_hist_${cdllimit}ms.png")
	opts+="filename='${lathistcdl}';"
	opts+="ptitle='I/O Latency Distribution, ${cdllimit} ms Limit I/Os'"

	${gcmd} -e "${opts}'" "${scriptdir}/lat_hist.gnuplot" > /dev/null 2>&1

	rm -f "${lathistnocdl}"
	rm -f "${lathistcdl}"
}

function process_cdl()
{
	local d="$1"
	local tmpf="$(mktemp)"

	echo "Processing CDL results (${cdllimit} ms limit)..."

	cd "${d}"

	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		echo "  QD=${qd}"
		${scriptdir}/../fio-prio-stats.sh \
			--save-priolat \
			--terse \
			--head "${qd}" \
			${qd}/randread.log_lat.log >> ${tmpf}
	done

	# Separate priorities
	resfnocdl="randread_nocdl.csv"
	resfcdl="randread_cdl.csv"

	grep "NONE" "${tmpf}" > "${resfnocdl}"
	grep -v "NONE" "${tmpf}" > "${resfcdl}"
	rm -f "${tmpf}"

	plot_cdl_iops "${resfnocdl}" "${resfcdl}"
	plot_cdl_lat_avg "${resfnocdl}" "${resfcdl}"
	plot_cdl_lat_p99 "${resfnocdl}" "${resfcdl}"
	plot_cdl_lat_hist

	cd ..
}


cd "${datadir}"

for d in $(ls); do

	if [ "${d}" == "nocdl" ]; then
		process_nocdl "${d}"
	elif [ "${d}" == "cdl" ]; then
		process_cdl "${d}"
	else
		echo "Unknown subdir ${d}"
		exit 1
	fi
done
