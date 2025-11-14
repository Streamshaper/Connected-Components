#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <matio.h>
#include <stdatomic.h>
#include "cca.h"
#include <omp.h>

#define GRAIN 2048

//Compile with: gcc -fopenmp -lm cc.c -O3 -o cc -lmatio

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

int main (int argc, char* argv[])
{
    open_matrix ("com-LiveJournal.mat"); // Check, missing free
    
    _Atomic int* labels = malloc (n_nodes*sizeof(_Atomic int));  // label of each node
    int* active = malloc (n_nodes*sizeof(int));  // active nodes
    int* next_active = malloc (n_nodes*sizeof(int)); // nodes that need to be woken up

    int iteration = 0;
    int changed = 1;

    n_active = n_nodes;

    double t0 = wall_time();
    
    initialize_labels (labels, active, n_nodes);

    while (n_active)
    {
        iteration++;

        for (int i = 0; i < n_nodes; ++i)
            next_active[i] = 0;

        #pragma omp parallel for 
        for (int block = 0; block < n_nodes; block += GRAIN) 
        {
            int block_end = block + GRAIN;
            if (block_end > n_nodes) block_end = n_nodes;

            for (int i = block; i < block_end; ++i) 
            {
                if (active[i] == 0) continue;
            
                int start = ind_ptr[i];
                int end = ind_ptr[i+1];

                if (start == end) continue;

                int min_label = atomic_load_explicit(&labels[indices[start]], memory_order_relaxed);

                for (int k = start+1; k < end; k++)
                {
                    int neigh_label = atomic_load_explicit(&labels[indices[k]], memory_order_relaxed);
                    if (neigh_label < min_label)
                        min_label = neigh_label;
                }

                if (min_label < atomic_load_explicit(&labels[i], memory_order_relaxed))
                {
                    atomic_store_explicit(&labels[i], min_label, memory_order_relaxed);
                    for (int k=start; k<end; k++)
                       next_active[indices[k]] = 1;
                }
            }
        }

        n_active = get_elements_from_array (next_active, n_nodes);
        print_update(iteration, n_active);
        
        int* temp = active;
        active = next_active;
        next_active = temp;
    }

    printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), wall_time()-t0);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);

    return 0;
}

void initialize_labels (_Atomic int* labels,int* active, int nodes)
{
    for (int i=0; i<nodes; i++)
    {    
        atomic_init(&labels[i], i+1);
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

int unique_elements (_Atomic int* labels)
{
    int sum = 0;
    int *temp = malloc (n_nodes*sizeof(int));
    int found = 0;

    temp[0] = atomic_load(&labels[0]);
    sum++;

    for (int k=1; k<n_nodes; k++)
    {    
        found = 0;
        for (int l=0; l<sum; l++)
            if (temp[l] == atomic_load(&labels[k]))
            {
                found = 1;
                break;
            }
        if (!found)
        {
            temp[sum] = atomic_load(&labels[k]);
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

    indices = malloc (m*sizeof(int));
    ind_ptr = malloc (n*sizeof(int));

    indices = (int*)A->ir;         // row indices
    ind_ptr = (int*)A->jc;         // column pointers
    n_nodes = n;
    n_elements = nnz/2;

    Mat_Close(matfp);

    printf ("Loaded matrix with %d nodes and %d elements.\n", n_nodes, n_elements);

}
