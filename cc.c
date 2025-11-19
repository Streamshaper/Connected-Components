#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <matio.h>
#include <stdatomic.h>
#include <unistd.h>
#include <pthread.h>
#include "cca.h"

typedef struct {
    int tid;
    int n_threads;
    int n_nodes;

    int* ind_ptr;
    int* indices;
    int* labels;
    int* active;
    int* next_active;

    int iteration;
    int local_active_count;
} thread_data_t;


double wall_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
}

// Global matrix data
int n_nodes;
int n_elements;
int* indices;
int* ind_ptr;

// Global control
int n_active;
int n_threads;
int bench_active = 0;

// Pthread work-stealing infrastructure
pthread_barrier_t start_barrier;
pthread_barrier_t end_barrier;

volatile int stop_flag = 0;
_Atomic int work_index;

int main (int argc, char* argv[])
{
    if (argc > 1 && atoi(argv[1])==1)
        bench_active = 1;

    n_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (argc > 2) {
        if (atoi(argv[2]) <= sysconf(_SC_NPROCESSORS_ONLN))
            n_threads = atoi(argv[2]);
    }

    open_matrix ("com-LiveJournal.mat");

    double t0 = wall_time();
    
    int* labels = malloc (n_nodes*sizeof(int));  
    int* active = malloc (n_nodes*sizeof(int));  
    int* next_active = malloc (n_nodes*sizeof(int)); 

    int iteration = 0;
    n_active = n_nodes;

    initialize_labels (labels, active, n_nodes);

    pthread_t threads[n_threads];
    thread_data_t td[n_threads];

    // Initialize the pthread barriers
    pthread_barrier_init(&start_barrier, NULL, n_threads + 1);
    pthread_barrier_init(&end_barrier,   NULL, n_threads + 1);

    // Create persistent workers
    for (int t = 0; t < n_threads; t++)
    {
        td[t].tid       = t;
        td[t].n_threads = n_threads;
        td[t].n_nodes   = n_nodes;

        td[t].ind_ptr     = ind_ptr;
        td[t].indices     = indices;
        td[t].labels      = labels;
        td[t].active      = active;
        td[t].next_active = next_active;

        td[t].iteration   = iteration;

        pthread_create(&threads[t], NULL, worker, &td[t]);
    }

    while (n_active > 0)
    {
        iteration++;

        // Publish iteration marker to workers
        for (int t = 0; t < n_threads; t++)
            td[t].iteration = iteration;

        // Reset global work pointer
        atomic_store(&work_index, 0);

        // Wake workers to start this iteration
        pthread_barrier_wait(&start_barrier);

        // Wait for them to finish
        pthread_barrier_wait(&end_barrier);

        // Reduction: sum per-thread local active counts
        int n_active_next = 0;
        for (int t = 0; t < n_threads; t++)
            n_active_next += td[t].local_active_count;

        n_active = n_active_next;

        if(!bench_active)
            print_update (iteration, n_active);

        // Swap active arrays
        int* temp = active;
        active = next_active;
        next_active = temp;
    }

    // Stop workers cleanly
    stop_flag = 1;
    pthread_barrier_wait(&start_barrier);
    for (int t = 0; t < n_threads; t++)
        pthread_join(threads[t], NULL);

    pthread_barrier_destroy(&start_barrier);
    pthread_barrier_destroy(&end_barrier);

    double t1 = wall_time();
    if (!bench_active)
        printf ("Total Connected Components: %d, found in %lf seconds!\n", 
                unique_elements(labels), t1-t0);
    else
        printf ("%lf", t1-t0);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);
    
    return 0;
}

void* worker(void* arg)
{
    thread_data_t* td = (thread_data_t*)arg;

    while (1)
    {
        // Wait for main to start new iteration
        pthread_barrier_wait(&start_barrier);

        if (stop_flag)
            break;

        int n_nodes   = td->n_nodes;
        int iteration = td->iteration;
        td->local_active_count = 0;

        // Work stealing: threads grab node indices dynamically
        int i = atomic_fetch_add(&work_index, 1);

        while (i < n_nodes)
        {
            if (td->active[i] == iteration)
            {
                int s = td->ind_ptr[i];
                int e = td->ind_ptr[i+1];
                if (s != e)
                {
                    int min_label = td->labels[ td->indices[s] ];

                    for (int k = s + 1; k < e; k++)
                    {
                        int l = td->labels[ td->indices[k] ];
                        if (l < min_label)
                            min_label = l;
                    }

                    if (min_label < td->labels[i])
                    {
                        td->labels[i] = min_label;

                        for (int k = s; k < e; k++)
                        {
                            int nb = td->indices[k];

                            if (td->next_active[nb] != iteration + 1)
                            {
                                td->next_active[nb] = iteration + 1;
                                td->local_active_count++;
                            }
                        }
                    }
                }
            }

            // Grab another unit of work
            i = atomic_fetch_add(&work_index, 1);
        }

        // Signal that this worker is finished
        pthread_barrier_wait(&end_barrier);
    }

    return NULL;
}

void initialize_labels (int* labels,int* active, int nodes)
{
    for (int i=0; i<nodes; i++)
    {    
        labels[i] = i+1;
        active[i] = 1;
    }
}

int unique_elements (int* labels)
{
    int sum = 0;
    int *temp = malloc (n_nodes*sizeof(int));
    int found = 0;

    temp[0] = labels[0];
    sum++;

    for (int k=1; k<n_nodes; k++)
    {    
        found = 0;
        for (int l=0; l<sum; l++)
            if (temp[l] == labels[k])
            {
                found = 1;
                l=sum;
            }
        if (!found)
        {
            temp[sum] = labels[k];
            sum++;
        }
    }
    free (temp);
    return sum;
            
}

void print_update (int iter, int n_active)
{
    printf("Iteration: %d || Still Active: %d nodes.\n", iter, n_active);
}

void open_matrix (char* name)
{
    mat_t *matfp = Mat_Open(name, MAT_ACC_RDONLY);
    if (!matfp) { fprintf(stderr,"Cannot open file\n"); exit(2); }

    matvar_t *problem = Mat_VarRead(matfp, "Problem");
    if (!problem || problem->class_type != MAT_C_STRUCT) { fprintf(stderr,"Problem struct missing\n"); exit(2); }

    matvar_t *Avar = Mat_VarGetStructFieldByName(problem, "A", 0);
    if (!Avar || Avar->class_type != MAT_C_SPARSE) { fprintf(stderr,"A is not sparse\n"); exit(2); }

    mat_sparse_t *A = (mat_sparse_t*)Avar->data;
    size_t m = Avar->dims[0], n = Avar->dims[1], nnz = A->nzmax;

    indices = malloc (nnz*sizeof(int));
    ind_ptr = malloc (n*sizeof(int));

    for (size_t q=0; q<nnz; q++)
        indices[q] = (int)A->ir[q];

    for (size_t q=0; q<n; q++)
        ind_ptr[q] = (int)A->jc[q];

    n_nodes = n;
    n_elements = nnz/2;

    Mat_Close(matfp);

    if (!bench_active)
        printf ("Loaded matrix with %d nodes and %d elements.\n", n_nodes, n_elements);
}
