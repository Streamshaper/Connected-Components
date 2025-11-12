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

//Compile with: clang -fopencilk -lm cc.c -O3 -o cc -lmatio

double wall_time() {
    struct timeval t;
    gettimeofday(&t, NULL);
    return t.tv_sec + t.tv_usec * 1e-6;
}

int n_nodes;
int n_elements;
int* indices;
int* ind_ptr;

int main (int argc, char* argv[])
{
    open_matrix ("com-LiveJournal.mat"); // Check, missing free
    int nworkers = __cilkrts_get_nworkers();
    printf("Running with %d Cilk workers\n", nworkers);
    _Atomic int *labels = malloc (n_nodes*sizeof(_Atomic int));  // label of each node
    _Atomic int *active = malloc (n_nodes*sizeof(_Atomic int));  // active nodes
    _Atomic int *next_active = malloc (n_nodes*sizeof(_Atomic int)); // nodes that need to be woken up

    int n_neigh = 0;
    int iteration = 0;
    int changed = 1;

    double t0 = wall_time();
    
    initialize_labels (labels, active, n_nodes);

    wsp_t start = wsp_getworkspan();

    while (changed)
    {
        iteration++;

        memset (next_active, 0, n_nodes*sizeof(_Atomic int));

        cilk_for (int i=0; i<n_nodes; i++)
        {
            if (atomic_load(&active[i]) == 0) continue;
            
            int min_label;
            int start;
            int end;
            start = ind_ptr[i];
            end = ind_ptr[i+1];

            if (start == end) continue;

            min_label = atomic_load(&labels[indices[start]]);

            for (int k = start+1; k < end; k++)
            {
                int neigh_label = atomic_load(&labels[indices[k]]);
                if (neigh_label < min_label)
                    min_label = neigh_label;
            }

            if (min_label < atomic_load(&labels[i]))
            {
                atomic_store_explicit(&labels[i], min_label, memory_order_relaxed);
                for (int k=start; k<end; k++)
                    atomic_store_explicit(&next_active[indices[k]], 1, memory_order_relaxed);
            }
        }

        print_update(next_active, iteration);
        
        _Atomic int* temp = active;
        active = next_active;
        next_active = temp;

        if (get_elements_from_array(active, n_nodes) == 0)
            changed = 0;

    }

    wsp_t end = wsp_getworkspan();
    wsp_t elapsed = wsp_sub(end, start);
    wsp_dump(elapsed, "my computation");

    printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), wall_time()-t0);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);

    return 0;
}

void initialize_labels (_Atomic int* labels,_Atomic int* active, int nodes)
{
    for (int i=0; i<nodes; i++)
    {    
        atomic_init(&labels[i], i+1);
        atomic_init(&active[i], 1);
    }
}


int get_elements_from_array (_Atomic int* array, int array_size)
{
    int sum = 0;

    for (int q=0; q<array_size; q++)
        if (atomic_load(&array[q]) != 0) 
            sum++;

    return sum;
}

void print_update (_Atomic int* still_active_labels, int iter)
{
    printf("Iteration: %d || ", iter);
    printf("Still Active: %d nodes.", get_elements_from_array(still_active_labels, n_nodes));
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

void print_final (_Atomic int* labels, int iterations)
{
    printf("Number Of Connected Components: %d.", unique_elements(labels));
}

void open_matrix (char* name)
{
    mat_t *matfp = Mat_Open(name, MAT_ACC_RDONLY);
    if (!matfp) { fprintf(stderr,"Cannot open file\n"); exit(2); }

    matvar_t *problem = Mat_VarRead(matfp, "Problem");
    if (!problem || problem->class_type != MAT_C_STRUCT) { fprintf(stderr,"Problem struct missing\n"); exit(2); }

    matvar_t *Avar = Mat_VarGetStructFieldByName(problem, "A", 0);
    if (!Avar || Avar->class_type != MAT_C_SPARSE) { fprintf(stderr,"A is not sparse\n"); exit(2); }

    mat_sparse_t *A = (mat_sparse_t*)Avar->data; // Correct way to access sparse data
    size_t m = Avar->dims[0], n = Avar->dims[1], nnz = A->nzmax;

    indices = malloc (m*sizeof(int));
    ind_ptr = malloc (n*sizeof(int));

    indices = (int*)A->ir;         // row indices
    ind_ptr = (int*)A->jc;           // column pointers
    n_nodes = n;
    n_elements = nnz/2;

    Mat_Close(matfp);

    printf ("Loaded matrix with %d nodes and %d elements.\n", n_nodes, n_elements);

}
