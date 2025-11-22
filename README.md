### Purpose
The code contained here is an implementation of a label propagation algorithm that calculates the number of strongly connected components in a graph, represented by a matrix, which is in the compressed sparse row (CSR) format. In this branch, the basic algorithm is parallelized using **OpenMP**. Place a `matrix.mat` file in the same directory as the source code and follow the steps below to run the program and calculate the number of strongly connected components of the graph this specific matrix represents.
### Dependencies 
These libraries are required: libc, libmatio. GCC installation usually includes libc and provides support for OpenMP. Install libmatio in Ubuntu Linux with: `sudo apt update` and then `sudo apt install libmatio-dev`.
### Compiling and running the program  
To create the executable run `make`, to remove it run `make clean`.

The correct syntax for running the program is `./cc {mode}`. When mode == 0 the program prints complete update messages regarding the completion of the calculation. When mode == 1, the program restricts all stdout printing, except from the total execution time (in seconds) before terminating.  
Example: `./cc 1` runs in "benchmark mode".  
Example: `./cc 0` prints updates about the calculation progress.

In order to run the program on x threads run `OMP_NUM_THREADS=x`.

### Measuring Performance
In order to calculate and print the average and minimum execution time of the program throughout a set of runs, use `./stopwatch`. Full syntax: `OMP_NUM_THREADS={num_of_threads} ./stopwatch ./cc {num_of_runs} 1`. Moreover, averaged CPU Utilization, maximum resident set size and voluntary context switches can be printed using `./metrics`. Full syntax: `OMP_NUM_THREADS={num_of_threads} ./metrics ./cc {num_of_runs} 1`. Example: `OMP_NUM_THREADS=2 ./stopwatch ./cc 100 1` will output the average and minimum execution time for 100 runs on 2 threads.

### Notes
The time that the program reports as execution time is the calculation time; the initial loading of the matrix in memory is not taken into account.
