#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <matio.h>
#include <cilk/cilk.h>
#include <stdatomic.h>
#include "cca.h"

//Compile with: gcc -I. -lm -lmatio -O3 -o cc, please don't!

int n_nodes;
int n_elements;
int* indices;
int* ind_ptr;

int main (int argc, char* argv[])
{
    open_matrix ("matrix.mat"); // Check, missing free

    _Atomic int *labels = malloc (n_nodes*sizeof(_Atomic int));  // label of each node
    int *next_labels = malloc (n_nodes*sizeof(int));  // label of each node
    _Atomic int *active = malloc (n_nodes*sizeof(_Atomic int));  // active nodes
    _Atomic int *next_active = malloc (n_nodes*sizeof(_Atomic int)); // nodes that need to be woken up
    int *neigh_labels = malloc (n_nodes*sizeof(int)); // neighbours' labels for a given node
    int min_label, start, end;
    int n_neigh = 0;
    int iteration = 0;
    int changed = 1;

    clock_t zero_t = clock();
    clock_t start_t = clock();
    clock_t end_t;
    
    initialize_labels (labels, next_labels, active, n_nodes);

    while (changed)
    {
        iteration++;

        memset (next_active, 0, n_nodes*sizeof(_Atomic int));

        cilk_for (int i=0; i<n_nodes; i++)
        {
            if (atomic_load(&active[i]) != 1) continue;

            start = ind_ptr[i];
            end   = ind_ptr[i+1];
            if (start == end) continue;

            min_label = atomic_load(&labels[indices[start]]);

            for (int k = start+1; k < end; k++)
                if (atomic_load(&labels[indices[k]]) < min_label)
                    min_label = atomic_load(&labels[indices[k]]);

            if (min_label < atomic_load(&labels[i]))
            {
                atomic_store(&labels[i], min_label);
                for (int k=start; k<end; k++)
                    atomic_store(&next_active[indices[k]], 1);
            }

            if (i%200000 == 0)
            {
                end_t = clock();
                printf ("Iteration completion: %.1f%% || Time elapsed: %.1lf seconds\n", 
                    100.0*i/n_nodes,(double)(end_t-start_t)/CLOCKS_PER_SEC);
            }
        }

        //memcpy (labels, next_labels, n_nodes*sizeof(int));

        print_update(next_active, iteration);
        
        _Atomic int* temp = active;
        active = next_active;
        next_active = temp;

        //reinitialize_matrices(next_active, n_nodes);  // once per iteration, compare with iteration number don't initialize (idea)

        //printf ("UC: %d\n", unique_elements(labels));

        if (get_elements_from_array(active, n_nodes) == 0)
            changed = 0;

    }


    printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), (double)(clock()-zero_t)/CLOCKS_PER_SEC);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);
    free(next_labels);
    free(neigh_labels);

    return 0;
}

void initialize_csr_matrix (int* ind_ptr, int* indices)
{
    int temp_ptr [] = {0, 2, 3, 5, 7, 9, 10, 11, 12, 14, 15, 16, 16};
    for (int q=0; q<n_elements; q++)
        ind_ptr[q] = temp_ptr[q];

    int temp_idx [] = {1, 4, 0, 7, 8, 4, 10, 0, 3, 6, 5, 2, 2, 9, 8, 3};
    for (int q=0; q<n_elements; q++)
        indices[q] = temp_idx[q];
}

void initialize_labels (_Atomic int* labels, int* next_labels, int* active, int nodes)
{
    for (int i=0; i<nodes; i++)
    {    
        atomic_init(&labels[i], i+1);
        next_labels[i] = i+1;
        active[i] = 1;
    }
}

void reinitialize_matrices (int* next_active, int nodes)
{
    for (int p=0; p<nodes; p++)
    {
        next_active[p] = 0;
    }
}

void reinitialize_neighboors (int* neigh_labels, int nodes)
{
    for (int p=0; p<nodes; p++)
        neigh_labels[p] = 0;
}

int get_elements_from_array (_Atomic int* array, int array_size)
{
    int sum = 0;

    for (int q=0; q<array_size; q++)
        if (atomic_load(&array[q]) != 0) 
            sum++;

    return sum;
}

void print_update (int* still_active_labels, int iter)
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

    indices = A->ir;         // row indices
    ind_ptr = A->jc;           // column pointers
    n_nodes = n;
    n_elements = nnz/2;

    Mat_Close(matfp);

    printf ("You've got %d nodes and %d elements in total.\n", n_nodes, n_elements);

}
