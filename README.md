**Dependencies**: libc, pthread, libmatio  
GCC installation usually includes libc and pthread. 
Install libmatio in Ubuntu Linux with: `sudo apt update` and then `sudo apt install libmatio-dev`    
### Creating and running the program  
To create the executable run `make`, to remove it run `make clean`.  
The correct syntax for running the program is `./cc {mode} {num_threads}`. When mode == 0 the program prints complete feedback messages. When mode == 1, the program restricts all stdout printing, except from the total execution time (in seconds) before terminating. num_threads should be set to the number of threads that should be spawned simultaneously during execution.  
Example: `./cc 1 4` runs in "benchmark mode" on 4 threads.  
In order to calculate and print the average and minimum execution time of the program throughout a set of runs, use `./script`.  
Full syntax: `./script ./cc {num_of_runs} {mode} {num_threads}`.
