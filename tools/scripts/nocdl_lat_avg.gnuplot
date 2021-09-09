#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

set title "I/O Latency Average"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "Latency (ms)"
set yrange [0:]

plot filename using 1:8 with lp title "Average latency" lc rgb"red"
