#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <matio.h>
#include <stdatomic.h>
#include <unistd.h>
#include <pthread.h>

#include "cca.h"

// ----------------------------------
// GLOBAL GRAPH STORAGE
// ----------------------------------
int n_nodes;
int n_elements;
int* ind_ptr;
int* indices;

// ----------------------------------
// RUNTIME CONTROL
// ----------------------------------
int n_threads;
int n_active;
int bench_active = 0;

volatile int stop_flag = 0;
_Atomic int work_index;

// ----------------------------------
// CUSTOM PORTABLE BARRIER
// ----------------------------------
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    int count;
    int waiting;
} my_barrier_t;

void my_barrier_init(my_barrier_t *b, int count) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->count   = count;
    b->waiting = 0;
}

void my_barrier_wait(my_barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    b->waiting++;
    if (b->waiting == b->count) {
        b->waiting = 0;
        pthread_cond_broadcast(&b->cond);
    } else {
        while (pthread_cond_wait(&b->cond, &b->mutex) != 0);
    }
    pthread_mutex_unlock(&b->mutex);
}

// Two global barriers
my_barrier_t start_barrier;
my_barrier_t end_barrier;

// ----------------------------------
// THREAD ARG STRUCT
// ----------------------------------
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
    unsigned char *local_next_active;   // per-thread buffer
} thread_data_t;


// ----------------------------------
// TIMING UTILITY
// ----------------------------------
double wall_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
}


// ----------------------------------
// FORWARD DECLARATIONS
// ----------------------------------
void* worker(void* arg);
void initialize_labels(int* labels, int* active, int nodes);
int unique_elements(int* labels);
void print_update(int iter, int n_active);
void open_matrix(char* name);


// ----------------------------------
// MAIN
// ----------------------------------
int main(int argc, char* argv[])
{
    if (argc > 1 && atoi(argv[1]) == 1)
        bench_active = 1;

    n_threads = sysconf(_SC_NPROCESSORS_ONLN);
    if (argc > 2) {
        int req = atoi(argv[2]);
        if (req > 0 && req <= n_threads)
            n_threads = req;
    }

    open_matrix("com-LiveJournal.mat");

    double t0 = wall_time();

    int* labels      = malloc(n_nodes * sizeof(int));
    int* active      = malloc(n_nodes * sizeof(int));
    int* next_active = malloc(n_nodes * sizeof(int));

    memset(next_active, 0, n_nodes * sizeof(int));

    int iteration = 0;
    n_active = n_nodes;

    initialize_labels(labels, active, n_nodes);

    // Create persistent threads
    pthread_t threads[n_threads];
    thread_data_t td[n_threads];

    my_barrier_init(&start_barrier, n_threads + 1);
    my_barrier_init(&end_barrier,   n_threads + 1);

    for (int t = 0; t < n_threads; t++)
    {
        td[t].tid           = t;
        td[t].n_threads     = n_threads;
        td[t].n_nodes       = n_nodes;

        td[t].ind_ptr       = ind_ptr;
        td[t].indices       = indices;
        td[t].labels        = labels;
        td[t].active        = active;
        td[t].next_active   = next_active;

        td[t].iteration     = iteration;
        td[t].local_active_count = 0;

        // Allocate private buffer, memset lazily per iteration
        td[t].local_next_active = malloc(n_nodes);
        memset(td[t].local_next_active, 0, n_nodes);

        pthread_create(&threads[t], NULL, worker, &td[t]);
    }



    // ----------------------------------
    // MAIN LOOP
    // ----------------------------------
    while (n_active > 0)
    {
        iteration++;

        for (int t = 0; t < n_threads; t++)
        {
            td[t].iteration     = iteration;
            td[t].active        = active;
            td[t].next_active   = next_active;
            memset(td[t].local_next_active, 0, n_nodes);
        }

        atomic_store(&work_index, 0);

        my_barrier_wait(&start_barrier);
        my_barrier_wait(&end_barrier);

        // Combine thread-local activity counts
        int n_active_next = 0;
        for (int t = 0; t < n_threads; t++)
            n_active_next += td[t].local_active_count;

        n_active = n_active_next;

        if (!bench_active)
            print_update(iteration, n_active);

        // Swap active buffers
        int* tmp = active;
        active = next_active;
        next_active = tmp;
    }


    // ----------------------------------
    // SHUTDOWN
    // ----------------------------------
    stop_flag = 1;
    my_barrier_wait(&start_barrier);

    for (int t = 0; t < n_threads; t++) {
        pthread_join(threads[t], NULL);
        free(td[t].local_next_active);
    }

    double t1 = wall_time();

    if (!bench_active)
        printf("Total Connected Components: %d, found in %lf seconds!\n",
               unique_elements(labels), t1 - t0);
    else
        printf("%lf", t1 - t0);

    free(ind_ptr);
    free(indices);
    free(labels);
    free(active);
    free(next_active);

    return 0;
}


// ----------------------------------
// WORKER THREAD
// ----------------------------------
void* worker(void* arg)
{
    thread_data_t* td = (thread_data_t*)arg;

    while (1)
    {
        my_barrier_wait(&start_barrier);

        if (stop_flag)
            break;

        int n_nodes   = td->n_nodes;
        int iteration = td->iteration;
        td->local_active_count = 0;

        unsigned char *local = td->local_next_active;

        // Parallel for using atomic index
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
                        int lbl = td->labels[td->indices[k]];
                        if (lbl < min_label)
                            min_label = lbl;
                    }

                    if (min_label < td->labels[i])
                    {
                        td->labels[i] = min_label;

                        for (int k = s; k < e; k++)
                        {
                            int nb = td->indices[k];
                            if (!local[nb])
                            {
                                local[nb] = 1;
                                td->local_active_count++;
                            }
                        }
                    }
                }
            }

            i = atomic_fetch_add(&work_index, 1);
        }

        // Merge into shared next_active buffer
        for (int j = 0; j < n_nodes; j++)
            if (local[j])
                td->next_active[j] = iteration + 1;

        my_barrier_wait(&end_barrier);
    }

    return NULL;
}


// ----------------------------------
// INITIALIZATION
// ----------------------------------
void initialize_labels(int* labels, int* active, int nodes)
{
    for (int i = 0; i < nodes; i++)
    {
        labels[i] = i + 1;
        active[i] = 1;
    }
}


// ----------------------------------
// COUNT UNIQUE LABELS
// ----------------------------------
int unique_elements(int* labels)
{
    int sum = 0;
    int *temp = malloc(n_nodes * sizeof(int));
    int found = 0;

    temp[0] = labels[0];
    sum++;

    for (int k = 1; k < n_nodes; k++)
    {
        found = 0;
        for (int l = 0; l < sum; l++)
            if (temp[l] == labels[k]) {
                found = 1;
                break;
            }
        if (!found)
            temp[sum++] = labels[k];
    }

    free(temp);
    return sum;
}


// ----------------------------------
// PRINT ITERATION STATUS
// ----------------------------------
void print_update(int iter, int n_active)
{
    printf("Iteration: %d || Still Active: %d nodes.\n", iter, n_active);
}


// ----------------------------------
// LOAD MATRIX MARKET FILE
// ----------------------------------
void open_matrix(char* name)
{
    mat_t *matfp = Mat_Open(name, MAT_ACC_RDONLY);
    if (!matfp) { fprintf(stderr,"Cannot open file\n"); exit(2); }

    matvar_t *problem = Mat_VarRead(matfp, "Problem");
    if (!problem || problem->class_type != MAT_C_STRUCT)
        { fprintf(stderr,"Problem struct missing\n"); exit(2); }

    matvar_t *Avar = Mat_VarGetStructFieldByName(problem, "A", 0);
    if (!Avar || Avar->class_type != MAT_C_SPARSE)
        { fprintf(stderr,"A is not sparse\n"); exit(2); }

    mat_sparse_t *A = (mat_sparse_t*)Avar->data;
    size_t m = Avar->dims[0], n = Avar->dims[1], nnz = A->nzmax;

    indices = malloc(nnz * sizeof(int));
    ind_ptr = malloc((n + 1) * sizeof(int));

    for (size_t q = 0; q < nnz; q++)
        indices[q] = (int)A->ir[q];

    for (size_t q = 0; q < n + 1; q++)
        ind_ptr[q] = (int)A->jc[q];

    n_nodes    = n;
    n_elements = nnz / 2;

    Mat_Close(matfp);

    if (!bench_active)
        printf("Loaded matrix with %d nodes and %d elements.\n", n_nodes, n_elements);
}
