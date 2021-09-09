#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

cdl = sprintf('%s limit I/Os', limit)

set title "I/O Rate"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "IOPS"
set yrange [0:]
set ytics 20

plot filename using 1:2 with lp title "No limit I/Os" lc rgb"red" dt "_.", \
     filename using 1:3 with lp title cdl lc rgb"red", \
     filename using 1:4 with lp title "All I/Os" lc rgb"blue"
