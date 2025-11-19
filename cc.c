#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <matio.h>
#include <cilk/cilk.h>
#include <stdatomic.h>
#include "cca.h"
#include <cilk/cilkscale.h> // Used for benchmarking
#include <cilk/cilk_api.h>

#define GRAIN 65536

double wall_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
}

int n_nodes;
int n_elements;
int* indices;
int* ind_ptr;
int n_active;
int bench_active = 0;

wsp_t start;
wsp_t end;

int main (int argc, char* argv[])
{
    if (argc > 1 && atoi(argv[1])==1)
        bench_active = 1;

    open_matrix ("com-LiveJournal.mat");
    
    double t0 = wall_time();

    int* labels = malloc (n_nodes*sizeof(int));  // label of each node
    int* active = malloc (n_nodes*sizeof(int));  // active nodes
    int* next_active = malloc (n_nodes*sizeof(int)); // nodes that need to be woken up

    int iteration = 0;
    int changed = 1;

    n_active = n_nodes;
    
    initialize_labels (labels, active, n_nodes);

    if (!bench_active)
        start = wsp_getworkspan();

    while (n_active)
    {
        iteration++;

        memset(next_active, 0, n_nodes * sizeof(*next_active)); // presumably faster than for and cilk_for



        cilk_for (int block = 0; block < n_nodes; block += GRAIN) 
        {
            int block_end = block + GRAIN;
            if (block_end > n_nodes) block_end = n_nodes;

            for (int i = block; i < block_end; ++i) 
            {
                if (active[i] == 0) continue;
            
                int start = ind_ptr[i];
                int end = ind_ptr[i+1];

                if (start == end) continue;

                int min_label = labels[indices[start]];

                for (int k = start+1; k < end; k++)
                {
                    int neigh_label = labels[indices[k]];
                    if (neigh_label < min_label)
                        min_label = neigh_label;
                }

                if (min_label < labels[i])
                {
                    labels[i] = min_label;
                    for (int k=start; k<end; k++)
                        if (!next_active[indices[k]])   // reduces cache-line ping-pong
                            next_active[indices[k]] = 1; 
                }
            }
        }

        n_active = get_elements_from_array (next_active, n_nodes);
        if (!bench_active)
            print_update(iteration, n_active);
        
        int* temp = active;
        active = next_active;
        next_active = temp;
    }
    if (!bench_active)
    {
        end = wsp_getworkspan();
        wsp_dump(wsp_sub(end, start), "my computation");
    }

    if(!bench_active)
        printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), wall_time()-t0);
    else
        printf ("%lf", wall_time()-t0);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);

    return 0;
}

void initialize_labels (int* labels,int* active, int nodes)
{
    cilk_for (int i=0; i<nodes; i++)
    {    
        labels[i] = i+1;
        active[i] = 1;
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
                break;
            }
        if (!found)
        {
            temp[sum] = labels[k];
            sum++;
        }
    }
    return sum;
            
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
