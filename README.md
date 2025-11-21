### Purpose
The code contained here is an implementation of a label propagation algorithm that calculates the number of strongly connected components in a graph, represented by a matrix, which is in the compressed sparse row (CSR) format. In this branch, the basic algorithm is parallelized using **Pthreads/POSIX Threads**. Place a `matrix.mat` file in the same directory as the source code and follow the steps below to run the program and calculate the number of strongly connected components of the graph this specific matrix represents.
### Dependencies 
These libraries are required: libc, pthread, libmatio. GCC installation usually includes the first two. Install libmatio in Ubuntu Linux with: `sudo apt update` and then `sudo apt install libmatio-dev`.
### Compiling and running the program  
To create the executable run `make`, to remove it run `make clean`.

The correct syntax for running the program is `./cc {mode} {num_threads}`. When mode == 0 the program prints complete update messages regarding the completion of the calculation. When mode == 1, the program restricts all stdout printing, except from the total execution time (in seconds) before terminating. num_threads should be set to the number of threads that should be spawned simultaneously during execution.  
Example: `./cc 1 4` runs in "benchmark mode" on 4 threads.

In order to calculate and print the average and minimum execution time of the program throughout a set of runs, use `./script`. Full syntax: `./script ./cc {num_of_runs} {mode} {num_threads}`.
