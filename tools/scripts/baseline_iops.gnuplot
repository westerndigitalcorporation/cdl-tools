#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

set title "Baseline I/O Rate"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "IOPS"
set yrange [0:]
set ytics 10

plot filename using 1:4 with lp title "IOPS" lc rgb"blue"
