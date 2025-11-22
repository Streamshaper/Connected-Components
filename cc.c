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

int n_nodes;
int* indices;
int* ind_ptr;
int n_active;
int n_threads;
int bench_active = 0;

int main (int argc, char* argv[])
{
    if (argc > 1 && atoi(argv[1])==1)
        bench_active = 1;

    n_threads = _SC_NPROCESSORS_ONLN;
    if (argc > 2)
        if (atoi(argv[2])<=_SC_NPROCESSORS_ONLN)
            n_threads = atoi(argv[2]);
    
    open_matrix ("matrix.mat");

    double t0 = wall_time();
    
    int* labels = malloc (n_nodes*sizeof(int));         // Label of each node
    int* active = malloc (n_nodes*sizeof(int));         // Active nodes
    int* next_active = malloc (n_nodes*sizeof(int));    // Nodes that need to be woken up in the next iteration

    int iteration = 0;  // Iteration counter
    n_active = n_nodes;

    initialize_labels (labels, active, next_active, n_nodes);

    pthread_t threads[n_threads];
    thread_data_t td[n_threads];

    while (n_active > 0)
    {
        iteration++;

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
        }

        for (int t = 0; t < n_threads; t++)
            pthread_create(&threads[t], NULL, worker, &td[t]);

        for (int t = 0; t < n_threads; t++)
            pthread_join(threads[t], NULL);

        int n_active_next = 0;
        for (int t = 0; t < n_threads; t++)
            n_active_next += td[t].local_active_count;      // Overestimation of n_active_next

        n_active = n_active_next;

        if(!bench_active)
            print_update (iteration, n_active);

        int* temp = active;
        active = next_active;
        next_active = temp;
    }

    double t1 = wall_time();
    if (!bench_active)
        printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), t1-t0);
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

    int tid       = td->tid;
    int n_threads = td->n_threads;
    int n_nodes   = td->n_nodes;

    int chunk = (n_nodes + n_threads - 1) / n_threads;
    int start_i = tid * chunk;
    int end_i   = start_i + chunk;
    if (end_i > n_nodes) end_i = n_nodes;

    td->local_active_count = 0;

    for (int i = start_i; i < end_i; i++)
    {
        if (td->active[i] != td->iteration) continue;

        int start = td->ind_ptr[i];
        int end = td->ind_ptr[i+1];
        if (start == end) continue;

        int min_label = td->labels[ td->indices[start] ];

        for (int k = start + 1; k < end; k++)
        {
            int l = td->labels[ td->indices[k] ];
            if (l < min_label)
                min_label = l;
        }

        if (min_label < td->labels[i])
        {
            td->labels[i] = min_label;

            for (int k = start; k < end; k++)
            {
                int nb = td->indices[k];

                if (td->next_active[nb] != td->iteration + 1)
                {
                    td->next_active[nb] = td->iteration + 1;
                    td->local_active_count++;
                }
            }
        }
    }

    return NULL;
}

void initialize_labels (int* labels, int* active, int* next_active, int nodes)
{
    for (int i=0; i<nodes; i++)
    {    
        labels[i] = i+1;
        active[i] = 1;
        next_active[i] = 0;
    }
}

int get_elements_from_array (int* array, int array_size)
{
    int sum = 0;

    for (int q=0; q<array_size; q++)
        if (array[q] != 0) 
            sum++;

    return sum;
}

void print_update (int iter, int n_active)
{
    printf("Iteration: %d || ", iter);
    printf("Still Active: %d nodes.", n_active);
    printf ("\n");
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

double wall_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
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
    ind_ptr = malloc ((n+1)*sizeof(int));

    for (size_t q=0; q<nnz; q++)
        indices[q] = (int)A->ir[q];

    for (size_t q=0; q<=n; q++)
        ind_ptr[q] = (int)A->jc[q];

    n_nodes = n;

    Mat_Close(matfp);

    if (!bench_active)
        printf ("Loaded matrix with %d nodes and %d edges.\n", n_nodes, (int)nnz/2);

}
