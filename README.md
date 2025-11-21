### Purpose
The code contained here is an implementation of a label propagation algorithm that calculates the number of strongly connected components in a graph, represented by a matrix, which is in the compressed sparse row (CSR) format. In this branch, the basic algorithm is executed **sequentially**. Place a `matrix.mat` file in the same directory as the source code and follow the steps below to run the program and calculate the number of strongly connected components of the graph this specific matrix represents.
### Dependencies 
These libraries are required: libc, libmatio. GCC installation usually includes libc. Install libmatio in Ubuntu Linux with: `sudo apt update` and then `sudo apt install libmatio-dev`.
### Compiling and running the program  
To create the executable run `make`, to remove it run `make clean`.

The correct syntax for running the program is `./cc {mode}`. When mode == 0 the program prints complete update messages regarding the completion of the calculation. When mode == 1, the program restricts all stdout printing, except from the total execution time (in seconds) before terminating.  
Example: `./cc 1` runs in "benchmark mode".

In order to calculate and print the average and minimum execution time of the program throughout a set of runs, use `./script`. Full syntax: `./script ./cc {num_of_runs} 1`.
