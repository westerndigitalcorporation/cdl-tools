#!/bin/bash

basedir="$(cd "$(dirname "$0")" && pwd)"
scriptdir="${basedir}/scripts"

. "${scriptdir}/bench_lib.sh"

require_program gnuplot

# defaults
gsave=0
w=860
h=600

function usage()
{
	local cmd="$(basename $0)"

	echo "Usage: ${cmd} [Options] <data dir>"
	echo "Options:"
	echo "  -h | --help  : Print this help message"
	echo "  --save       : Save plots to png files instead of displaying them"
	echo "  --w <width>  : Set plot width (default: ${w})"
	echo "  --h <height> : Set plot height (default: ${h})"
}

# Parse command line
if [ $# -lt 1 ]; then
	usage "$0"
	exit 1
fi

while [ "${1#-}" != "$1" ]; do
	case "$1" in
	-h | --help)
		usage "$0"
		exit 0
		;;
	--save)
		gsave=1
		;;
	--w)
		w=$2
		shift
		;;
	--h)
		h=$2
		shift
		;;

	-*)
		echo "unknow option $1"
		exit 1
		;;
	esac
	shift
done

gcmd="gnuplot"
if [ ${gsave} -eq 0 ]; then
	gcmd+=" -p"
fi

datadir="$(cd "$1" && pwd)"
basedir="$(pwd)"
plotdir="${datadir}/plots"
mkdir -p "${plotdir}" || exit_failed "Create plot directory ${plotdir} failed"
rm -f "${plotdir}/*.png"

function gopts()
{
        local png="$1"
        local opts=""

        if [ ${gsave} -eq 1 ]; then
		opts="set terminal pngcairo dashed size ${w},${h};"
        	opts+="set output '${png}';"
	else
		opts="set terminal qt size ${w},${h};"
	fi
        echo "${opts}"
}

function fio_iops()
{
	local fiolog="$1"

	grep "  read: IOPS=" "${fiolog}" | cut -d'=' -f2 | cut -d',' -f1
}

function cdl_limit()
{
        local dld="$1"

	grep "    duration guideline       :" cdl_descriptors | \
		head -n ${dld} | tail -n 1 | cut -d':' -f2 | cut -d' ' -f2
}

function plot_prefix()
{
	if [ "$1" == "cdlsingle" ] || [ "$1" == "cdlmulti" ]; then
		echo "cdl"
	else
		echo "$1"
	fi
}

function plot_iops()
{
        local csvf="$1"
        local prefix="$2"
        local baseline_resf="$3"
        local opts

	echo "  Plotting IOPS"

	opts=$(gopts "${plotdir}/${prefix}_iops.png")
        opts+="filename='${csvf}';"
	if [ -f "${baseline_resf}" ]; then
        	opts+="baseline='${baseline_resf}';"
	fi

	prefix="$(plot_prefix ${prefix})"
	${gcmd} -e "${opts}" "${scriptdir}/${prefix}_iops.gnuplot" > /dev/null 2>&1
}

function plot_lat_avg()
{
        local csvf="$1"
        local prefix="$2"
        local baseline_resf="$3"
        local opts

	echo "  Plotting latency average"

	opts=$(gopts "${plotdir}/${prefix}_lat_avg.png")
        opts+="filename='${csvf}';"
	if [ -f "${baseline_resf}" ]; then
        	opts+="baseline='${baseline_resf}';"
	fi

	prefix="$(plot_prefix ${prefix})"
	${gcmd} -e "${opts}" "${scriptdir}/${prefix}_lat_avg.gnuplot" > /dev/null 2>&1
}

function plot_lat_p99()
{
        local csvf="$1"
        local prefix="$2"
        local baseline_resf="$3"
        local opts

	echo "  Plotting latency 99th percentile"

        opts=$(gopts "${plotdir}/${prefix}_lat_p99.png")
        opts+="filename='${csvf}';"
	if [ -f "${baseline_resf}" ]; then
        	opts+="baseline='${baseline_resf}';"
	fi

	prefix="$(plot_prefix ${prefix})"
        ${gcmd} -e "${opts}" "${scriptdir}/${prefix}_lat_p99.gnuplot" > /dev/null 2>&1
}

function concat_qd_files()
{
	local outfile="$1"
	local class="$2"
	local level="$3"
	local hint="$4"
	local latfiles=""

	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		latfiles+=" ${qd}/priolat.${class}.${level}.${hint}.log"
	done

	paste -d',' ${latfiles} > "${outfile}"
}

function plot_baseline_lat_pdf()
{
	local csvf="lat_pdf.csv"
	local latfiles=""

	echo "  Plotting latency distribution"

	# Concatenate the sorted IO latency files
	concat_qd_files "${csvf}" "NONE" "0" "0"

	opts=$(gopts "${plotdir}/baseline_lat_pdf.png")
        opts+="filename='${csvf}';"
	opts+="ptitle='Baseline I/O Latency Probability Distribution'"

	prefix="$(plot_prefix ${prefix})"
	${gcmd} -e "${opts}" "${scriptdir}/lat_pdf.gnuplot" > /dev/null 2>&1
}

function process_baseline()
{
	local d="$1"
	local i=0

	echo "Processing baseline results"

	cd "${d}"

	resf="randread.csv"
	rm -rf "${resf}"

	echo "Baseline" >> "${resf}"

	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		if [ $i -eq 0 ]; then
			echo -n "  QD=${qd}"
		else
			echo -n ", QD=${qd}"
		fi
		i=$(( i + 1 ))
		${scriptdir}/../cdl_prio_stats.sh \
			--save-priolat \
			--terse \
			--head "${qd}" \
			${qd}/randread.log_lat.log >> ${resf}
	done
	echo ""

	plot_iops "${resf}" "baseline"
	plot_lat_avg "${resf}" "baseline"
	plot_lat_p99 "${resf}" "baseline"
	plot_baseline_lat_pdf

	cd ..
}

function plot_cdl_lat_pdf()
{
	local prefix="$1"
	local class="$2"
	local level="$3"
	local hint="$4"
	local csvf

	echo "  Plotting latency distribution, class ${class}, level ${level}, hint ${hint}"

	if [ "${class}" == "NONE" ]; then
		pngname="${plotdir}/${prefix}_lat_pdf_nolimit.png"
		ptitle="Command Duration Limits I/O Latency Distribution, No Limit I/Os"
		csvf="lat_pdf_nolimit.csv"
	else
		limit="$(cdl_limit ${hint})"
		pngname="${plotdir}/${prefix}_lat_pdf_${limit}ms.png"
		ptitle="Command Duration Limits I/O Latency Distribution, ${limit} ms Limit I/Os"
		csvf="lat_pdf_${limit}.csv"
	fi

	# Concatenate the sorted IO latency files
	concat_qd_files "${csvf}" "${class}" "${level}" "${hint}"

	opts=$(gopts "${pngname}")
	opts+="ptitle='${ptitle}';"
	opts+="filename='${csvf}'"

	${gcmd} -e "${opts}" "${scriptdir}/lat_pdf.gnuplot" > /dev/null 2>&1
}

function process_cdl()
{
	local d="$1"
	local tmpf="$(mktemp)"
	local priotmpf="$(mktemp)"
	declare -a classes=()
	declare -a levels=()
	declare -a hints=()
	local nrprios=0
	local i=0

	cd "${d}"

	if [ "${d}" == "cdlsingle" ]; then
		echo "Processing CDL results, single limit..."
	else
		echo "Processing CDL results, multiple limits..."
	fi

	resf="randread.csv"
	rm -rf "${resf}"

	echo "Total IOPS (all limits)" >> "${resf}"

	# Process all QDs, generating the total IOPS as we go
	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		if [ $i -eq 0 ]; then
			echo -n "  QD=${qd}"
		else
			echo -n ", QD=${qd}"
		fi
		i=$(( i + 1 ))
		${scriptdir}/../cdl_prio_stats.sh \
			--save-priolat \
			--terse \
			--head "${qd}" \
			${qd}/randread.log_lat.log >> ${tmpf}

		echo "${qd},ALL,0,0,$(fio_iops ${qd}/fio.log)" >> ${resf}
	done
	echo ""

	echo "" >> "${resf}"
	echo "" >> "${resf}"

	# resf already contains the total IOPS data block. Now add
	# one data block for the "no limit" I/Os and one per CDL limit found
	grep "NONE,0,0," "${tmpf}" > "${priotmpf}"
	if [ $(cat "${priotmpf}" | wc -l) -ne 0 ]; then
		echo "No limit I/Os" >> "${resf}"
		classes[${nrprios}]="NONE"
		levels[${nrprios}]="0"
		hints[${nrprios}]="0"
		cat "${priotmpf}" >> "${resf}"
		echo "" >> "${resf}"
		echo "" >> "${resf}"

		nrprios=$(( nrprios + 1 ))
	fi

	for i in 1 2 3 4 5 6 7; do
		grep "BE,0,${i}," "${tmpf}" > "${priotmpf}"
        	if [ $(cat "${priotmpf}" | wc -l) -ne 0 ]; then
			classes[${nrprios}]="BE"
			levels[${nrprios}]="0"
			hints[${nrprios}]="${i}"
			echo "$(cdl_limit ${i})ms limit I/Os" >> "${resf}"
                	cat "${priotmpf}" >> "${resf}"
                	echo "" >> "${resf}"
                	echo "" >> "${resf}"

			nrprios=$(( nrprios + 1 ))
		fi
	done

	rm -f "${tmpf}"
	rm -f "${priotmpf}"

	plot_iops "${resf}" "${d}" "$2"
	plot_lat_avg "${resf}" "${d}" "$2"
	plot_lat_p99 "${resf}" "${d}" "$2"

	for((i=0;i<${nrprios};i++)); do
		plot_cdl_lat_pdf "${d}" "${classes[$i]}" "${levels[$i]}" "${hints[$i]}"
	done

	cd ..
}

function plot_ncq_prio_lat_pdf()
{
	local class="$1"
	local level="$2"
	local hint="$3"
	local csvf=""

	echo "  Plotting latency distribution, class ${class}, level ${level}, hint ${hint}"

	if [ "${class}" == "NONE" ]; then
		pngname="${plotdir}/ncqprio_lat_pdf_lopri.png"
		ptitle="NCQ Priority I/O Latency Distribution, Low Priority I/Os"
		csvf="lat_pdf_lopri.csv"
	else
		pngname="${plotdir}/ncqprio_lat_pdf_hipri.png"
		ptitle="NCQ Priority I/O Latency Distribution, High Priority I/Os"
		csvf="lat_pdf_hipri.csv"
	fi

	# Concatenate the sorted IO latency files
	concat_qd_files "${csvf}" "${class}" "${level}" "${hint}"

	opts=$(gopts "${pngname}")
	opts+="ptitle='${ptitle}';"
	opts+="filename='${csvf}'"

	${gcmd} -e "${opts}" "${scriptdir}/lat_pdf.gnuplot" > /dev/null 2>&1
}

function process_ncq_prio()
{
	local d="$1"
	local tmpf="$(mktemp)"
	local priotmpf="$(mktemp)"
	declare -a classes
	declare -a levels
	declare -a hints
	local nrprios=0
	local i=0

	cd "${d}"

	echo "Processing NCQ priority results..."

	resf="randread.csv"
	rm -rf "${resf}"

	echo "Total IOPS (low + high)" >> "${resf}"

	# Process all QDs, generating the total IOPS as we go
	qds=($(ls -d [123456789]* | sort -n))
	for qd in ${qds[*]}; do
		if [ $i -eq 0 ]; then
			echo -n "  QD=${qd}"
		else
			echo -n ", QD=${qd}"
		fi
		i=$(( i + 1 ))
		${scriptdir}/../cdl_prio_stats.sh \
			--save-priolat \
			--terse \
			--head "${qd}" \
			${qd}/randread.log_lat.log >> ${tmpf}

		echo "${qd},ALL,0,0,$(fio_iops ${qd}/fio.log)" >> ${resf}
	done
	echo ""

	echo "" >> "${resf}"
	echo "" >> "${resf}"

	# resf already contains the total IOPS data block. Now add
	# one data block for the low and high priority I/Os.
	grep "NONE" "${tmpf}" > "${priotmpf}"
	if [ $(cat "${priotmpf}" | wc -l) -ne 0 ]; then
		echo "Low priority I/Os" >> "${resf}"
		classes[${nrprios}]="NONE"
		levels[${nrprios}]="0"
		hints[${nrprios}]="0"
		cat "${priotmpf}" >> "${resf}"
		echo "" >> "${resf}"
		echo "" >> "${resf}"

		nrprios=$(( nrprios + 1 ))
	fi

	grep "RT" "${tmpf}" > "${priotmpf}"
	if [ $(cat "${priotmpf}" | wc -l) -ne 0 ]; then
		echo "High Priority I/Os" >> "${resf}"
		classes[${nrprios}]="RT"
		levels[${nrprios}]="0"
		hints[${nrprios}]="0"
		cat "${priotmpf}" >> "${resf}"
		echo "" >> "${resf}"
		echo "" >> "${resf}"

		nrprios=$(( nrprios + 1 ))
	fi

	rm -f "${tmpf}"
	rm -f "${priotmpf}"

	plot_iops "${resf}" "ncqprio" "$2"
	plot_lat_avg "${resf}" "ncqprio" "$2"
	plot_lat_p99 "${resf}" "ncqprio" "$2"

	for((i=0;i<${nrprios};i++)); do
		plot_ncq_prio_lat_pdf "${classes[$i]}" "${levels[$i]}" "${hints[$i]}"
	done

	cd ..
}

cd "${datadir}"

for d in baseline ncqprio cdlsingle cdlmulti; do

	if [ ! -d "${d}" ]; then
		continue;
	fi

	if [ "${d}" == "baseline" ]; then
		process_baseline "${d}"
	elif [ "${d}" == "ncqprio" ]; then
		process_ncq_prio "${d}" "$(pwd)/baseline/randread.csv"
	else 
		process_cdl "${d}" "$(pwd)/baseline/randread.csv"
	fi
done
