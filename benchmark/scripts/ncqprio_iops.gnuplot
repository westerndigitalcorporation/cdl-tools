#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set datafile separator ","
set key on left
set border 3
set grid

set title "NCQ Priority I/O Rate"

set xlabel "I/O Queue Depth"
set xtics 8
set ylabel "IOPS"
set yrange [0:]
set ytics 10

# Total, low, high
plot baseline using 1:5 with lp title "Baseline" lc rgb"blue", \
     filename index 0 using 1:5 with lp title columnheader(1) lc rgb"violet", \
     filename index 1 using 1:5 with lp title columnheader(1) lc rgb"red" dt "_.", \
     filename index 2 using 1:5 with lp title columnheader(1) lc rgb"red"
