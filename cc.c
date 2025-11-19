#define BENCHMARK 0 // When set to 1, only the total time of computation is printed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <matio.h>
#include "cca.h"

int n_nodes;
int n_elements;
int* indices;
int* ind_ptr;

int main (int argc, char* argv[])
{
    open_matrix ("com-LiveJournal.mat");

    clock_t zero_t = clock();

    int *labels = malloc (n_nodes*sizeof(int));  // label of each node
    int *active = malloc (n_nodes*sizeof(int));  // active nodes
    int *next_active = malloc (n_nodes*sizeof(int)); // nodes that need to be woken up
   
    int min_label, start, end;
    int iteration = 0;
    int changed = 0;
    
    initialize_labels (labels, active, n_nodes);

    while (1)
    {
        iteration++;
        changed = 0;

        for (int i=0; i<n_nodes; i++)
        {
            if (active[i] != 1) continue;

            start = ind_ptr[i];
            end   = ind_ptr[i+1];
            if (start == end) continue;

            min_label = labels[indices[start]];

            for (int k = start+1; k < end; k++)
                if (labels[indices[k]] < min_label)
                    min_label = labels[indices[k]];

            if (min_label < labels[i])
            {
                labels[i] = min_label;
                changed = 1;
                for (int k=start; k<end; k++)
                    next_active[indices[k]] = 1;
            }
        }
        if (!BENCHMARK)
            print_update(labels, next_active, iteration);

        if (!changed) break;
        
        int* temp = active;
        active = next_active;
        next_active = temp;

        reinitialize_matrices(next_active, n_nodes);  // once per iteration, compare with iteration number don't initialize (idea)
    }

    if (!BENCHMARK)
        printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), (double)(clock()-zero_t)/CLOCKS_PER_SEC);
    else
        printf ("%lf", (double)(clock()-zero_t)/CLOCKS_PER_SEC);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);

    return 0;
}

void initialize_labels (int* labels, int* active, int nodes)
{
    for (int i=0; i<nodes; i++)
    {    
        labels[i] = i+1;
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

int get_elements_from_array (int* array, int array_size)
{
    int sum = 0;

    for (int q=0; q<array_size; q++)
        if (array[q] != 0) 
            sum++;

    return sum;
}

void print_update (int* labels, int* still_active_labels, int iter)
{
    printf("Iteration: %d || ", iter);
    printf("Still Active: %d nodes.", get_elements_from_array(still_active_labels, n_nodes));
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

    mat_sparse_t *A = (mat_sparse_t*)Avar->data; // Correct way to access sparse data
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
    if (!BENCHMARK)
        printf ("The graph has %d nodes and %d elements in total.\n", n_nodes, n_elements);

}
