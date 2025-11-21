### Purpose
The code contained here is an implementation of a label propagation algorithm that calculates the number of strongly connected components in a graph, represented by a matrix, which is in the compressed sparse row (CSR) format. In this branch, the basic algorithm is parallelized using **OpenCilk**. Place a `matrix.mat` file in the same directory as the source code and follow the steps below to run the program and calculate the number of strongly connected components of the graph this specific matrix represents.
### Dependencies 
OpenCilk should be installed in the system beforehand. Visit the [official site](https://www.opencilk.org/doc/users-guide/install/) for instructions. These libraries are required: libc, libmatio. Clang installation usually includes libc. Install libmatio in Ubuntu Linux with: `sudo apt update` and then `sudo apt install libmatio-dev`.
### Compiling and running the program  
To create the executable run `make`, to remove it run `make clean`. To create the executable without OpenCilk's cilkscale's capabilities run `make bench`

The correct syntax for running the program is `./cc {mode}`. When mode == 0 the program prints complete update messages regarding the completion of the calculation. When mode == 1, the program restricts all stdout printing, except from the total execution time (in seconds) before terminating. Combine `make` with mode 0 and `make bench` with mode 1, in order to allow or exclude all printouts simultaneously.  
Example: `make bench`, then `./cc 1` runs in "benchmark mode".  
Example: `make`, then `./cc 0` prints updates about the calculation progress and after completion shows metrics relevant to parallelism a) for the while loop b)for the whole program.

In order to run the program on x threads run `CILK_NWORKERS=x`.

In order to calculate and print the average and minimum execution time of the program throughout a set of runs, use `./script`. Full syntax: `CILK_NWORKERS={num_of_threads} ./script ./cc {num_of_runs} 1`.
