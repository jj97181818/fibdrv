make check
sudo insmod fibdrv.ko
gcc test_Fibonacci.c -o test && sudo ./test > test.txt && gnuplot plot.gp