#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

cdl = sprintf('%s limit I/Os', limit)

set title "I/O Latency Average"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "Latency (ms)"
set yrange [0:]

plot f1 using 1:8 with lp title "No limit I/Os" lc rgb"red" dt "_.", \
     f2 using 1:8 with lp title cdl lc rgb"red"
