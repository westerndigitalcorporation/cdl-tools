#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

set title "Command Duration Limits I/O latency 99th Percentile"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "Latency (ms)"
set yrange [0:]
set ytics 50

plot baseline using 1:22 with lp title "Baseline" lc rgb"blue", \
     filename index 1 using 1:22 with lp title columnheader(1) lc rgb"red" dt "_.", \
     filename index 2 using 1:22 with lp title columnheader(1) lc rgb"red", \
     filename index 3 using 1:22 with lp title columnheader(1) lc rgb"green", \
     filename index 4 using 1:22 with lp title columnheader(1) lc rgb"magenta", \
     filename index 5 using 1:22 with lp title columnheader(1) lc rgb"cyan"
