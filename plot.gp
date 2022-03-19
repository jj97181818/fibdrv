set title "Runtime"
set xlabel "nth Fibonacci number"
set ylabel "time(ns)"
set terminal png font " Times_New_Roman,12 "
set output "statistic.png"
set xtics 0, 10, 100
set key left 

plot \
"test.txt" using 1:2 with linespoints linewidth 2 title "iterative", \
"test.txt" using 1:3 with linespoints linewidth 2 title "fast doubling", \
"test.txt" using 1:4 with linespoints linewidth 2 title "fast doubling with clz", \
