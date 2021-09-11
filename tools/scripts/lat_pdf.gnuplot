#
# Copyright (C) 2021 Western Digital Corporation or its affiliates.
#

set key on right
set border 3
set grid
set datafile separator ","

stats filename

# 2ms per interval
binw=2

bin_number(x,w) = floor(x/w)
rounded(x,w) = w * ( bin_number(x,w) + 0.5 )

hist1 = 'using (rounded($1,binw)):(100.0/STATS_records) smooth frequency with l'
hist2 = 'using (rounded($2,binw)):(100.0/STATS_records) smooth frequency with l'
hist4 = 'using (rounded($3,binw)):(100.0/STATS_records) smooth frequency with l'
hist8 = 'using (rounded($4,binw)):(100.0/STATS_records) smooth frequency with l'
hist16 = 'using (rounded($5,binw)):(100.0/STATS_records) smooth frequency with l'
hist24 = 'using (rounded($6,binw)):(100.0/STATS_records) smooth frequency with l'
hist32 = 'using (rounded($7,binw)):(100.0/STATS_records) smooth frequency with l'
hist48 = 'using (rounded($8,binw)):(100.0/STATS_records) smooth frequency with l'
hist64 = 'using (rounded($9,binw)):(100.0/STATS_records) smooth frequency with l'

set style fill solid 0.5

set title ptitle
set xlabel "Latency (ms)"
set ylabel "Percentage"
set yrange [0:]

plot filename @hist1 title "I/O depth=1", \
     filename @hist2 title "I/O depth=2", \
     filename @hist4 title "I/O depth=4", \
     filename @hist8 title "I/O depth=8", \
     filename @hist16 title "I/O depth=16", \
     filename @hist24 title "I/O depth=24", \
     filename @hist32 title "I/O depth=32", \
     filename @hist48 title "I/O depth=48", \
     filename @hist64 title "I/O depth=64"
