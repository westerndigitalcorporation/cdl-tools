#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

set title "I/O Rate"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "IOPS"
set yrange [0:]
set ytics 20

plot filename index 0 using 1:4 with lp title columnheader(1) lc rgb"violet", \
     filename index 1 using 1:4 with lp title columnheader(1) lc rgb"red" dt "_.", \
     filename index 2 using 1:4 with lp title columnheader(1) lc rgb"red", \
     filename index 3 using 1:4 with lp title columnheader(1) lc rgb"green"
