#!/bin/sh

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
# $1: a prio value (0x....)
#
function get_prio_class()
{
	echo $(( $1 >> 13 ))
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
	4)
		echo "CDL";;
	*)
		echo "??";;
	esac
}

#
# $1: a prio value (0x....)
#
function get_prio_level()
{
	echo $(( $1 & 0x1FFF ))
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
        start=$(head -1 "${fiolatlog}" | cut -d',' -f1)
        end=$(tail -1 "${fiolatlog}" | cut -d',' -f1)

	echo "$(( end - start ))"
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
