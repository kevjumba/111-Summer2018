#! /usr/bin/gnuplot
# NAME: Kevin Zhang
# EMAIL: kevin.zhang.13499@gmail.com
# ID: 104939334
# purpose:
#    generate data reduction graphs for the multi-threaded list project
#
# input: lab2_list.csv
#   1. test name
#   2. # threads
#   3. # iterations per thread
#   4. # lists
#   5. # operations performed (threads x iterations x (ins + lookup + delete))
#   6. run time (ns)
#   7. run time per operation (ns)
#
# output:
#   lab2_list-1.png ... cost per operation vs threads and iterations
#   lab2_list-2.png ... threads and iterations that run (un-protected) w/o failure
#   lab2_list-3.png ... threads and iterations that run (protected) w/o failure
#   lab2_list-4.png ... cost per operation vs number of threads
#
# Note:
#   Managing data is simplified by keeping all of the results in a single
#   file.  But this means that the individual graphing commands have to
#   grep to select only the data they want.
#
#   Early in your implementation, you will not have data for all of the
#   tests, and the later sections may generate errors for missing data.
#
# general plot parameters
set terminal png
set datafile separator ","

# how many threads/iterations we can run without failure (w/o yielding)
set title "List-1: Throughput vs Number of Threads"
set xlabel "# of Threads"
set logscale x 2
set xrange [0.5:]  #xrange has to be greater than 0 for logscale
set ylabel "Throughput (operations/s)"
set logscale y 10
set output 'lab2b_1.png'

# throughput vs number of threads where throughput = 1 billion divided by the avg time/op in ns
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'Mutex' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
    title 'Spin-Lock' with linespoints lc rgb 'green'


set title "List-2: Wait for Lock and Average Time per Operation vs Number of Threads"
set xlabel "# of Threads"
set logscale x 2
set xrange [0.5:]
set ylabel "Time (ns)"
set logscale y 10
set output 'lab2b_2.png'
# note that unsuccessful runs should have produced no output
plot \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($7) \
    title 'Average Time per Operation' with linespoints lc rgb 'red', \
     "< grep -e 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):($8) \
    title 'Average Wait Time for Lock' with linespoints lc rgb 'green'


set title "List-3: Iterations that run without failure"
set logscale x 2
set xrange [0.5:]  #xrange has to be greater than 0 for logscale
set xlabel "Threads"
set ylabel "successful iterations" #xrange has to be greater than 0 for logscale
set logscale y 10
set output 'lab2b_3.png'
plot \
    "< grep 'list-id-none,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
    with points lc rgb "red" title "unprotected", \
    "< grep 'list-id-m,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
    with points lc rgb "blue" title "mutex", \
    "< grep 'list-id-s,[0-9]*,[0-9]*,4,' lab2b_list.csv" using ($2):($3) \
    with points lc rgb "green" title "spin lock"
#
# "no valid linespoints" is possible if even a single iteration can't run
#

# unset the kinky x axis
set title "List-4: Aggregated Throughput vs. The Number of Threads(Mutex)"
set logscale x 2
set xrange [0.5:] #xrange has to be greater than 0 for logscale
set xlabel "Threads"
set ylabel "Throughput(operations/sec)"
set logscale y 10
set output 'lab2b_4.png'
plot \
        "< grep 'list-none-m,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '1 list' with linespoints lc rgb 'red', \
        "< grep 'list-none-m,[0-9]*,1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '4 lists' with linespoints lc rgb 'green', \
        "< grep 'list-none-m,.[0-9]*,1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '8 lists' with linespoints lc rgb 'blue', \
        "< grep 'list-none-m,[0-9]*,1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '16 lists' with linespoints lc rgb 'orange'


# unset the kinky x axis
set title "List-5: Aggregated Throughput vs. The Number of Threads(Spin Lock)"
set logscale x 2
set xrange [0.5:] #xrange has to be greater than 0 for logscale
set xlabel "Threads"
set ylabel "Throughput(operations/sec)"
set logscale y 10
set output 'lab2b_5.png'
plot \
        "< grep 'list-none-s,[0-9]*,1000,1,' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '1 list' with linespoints lc rgb 'red', \
        "< grep 'list-none-s,[0-9]*,1000,4' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '4 lists' with linespoints lc rgb 'green', \
        "< grep 'list-none-s,.[0-9]*,1000,8' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '8 lists' with linespoints lc rgb 'blue', \
        "< grep 'list-none-s,[0-9]*,1000,16' lab2b_list.csv" using ($2):(1000000000/($7)) \
        title '16 lists' with linespoints lc rgb 'orange'
