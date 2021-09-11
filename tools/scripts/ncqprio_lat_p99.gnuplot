#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

set title "NCQ Priority I/O latency 99th Percentile"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "Latency (ms)"
set yrange [0:]

plot filename index 1 using 1:21 with lp title columnheader(1) lc rgb"red" dt "_.", \
     filename index 2 using 1:21 with lp title columnheader(1) lc rgb"red"
