#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <matio.h>
#include "cca.h"

//Compile with: gcc -I. -lm -lmatio -O3 -o cc

int n_nodes;
int n_elements;
int* indices;
int* ind_ptr;

int main (int argc, char* argv[])
{
    open_matrix ("matrix.mat");

    int *labels = malloc (n_nodes*sizeof(int));  // label of each node
    int *active = malloc (n_nodes*sizeof(int));  // active nodes
    int *next_active = malloc (n_nodes*sizeof(int)); // nodes that need to be woken up
    int *neigh_labels = malloc (n_nodes*sizeof(int)); // neighbours' labels for a given node
    int min_label, start, end;
    int n_neigh = 0;
    int iteration = 0;
    int changed = 0;

    clock_t start_t = clock();
    clock_t end_t;
    clock_t zero_t = clock();
    initialize_labels (labels, active, n_nodes);
    
    //free the memory afterwards

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

            if (i%200000 == 0)
            {
                end_t = clock();
                printf ("Iteration completion: %.1f%% || Time elapsed: %.1lf seconds\n", 
                    100.0*i/n_nodes,(double)(end_t-start_t)/CLOCKS_PER_SEC);
            }
        }

        print_update(labels, next_active, iteration);

        if (!changed) break;
        
        int* temp = active;
        active = next_active;
        next_active = temp;

        reinitialize_matrices(next_active, n_nodes);  // once per iteration, compare with iteration number don't initialize (idea)

        printf ("UC: %d\n", unique_elements(labels));
    }


    printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), (double)(clock()-zero_t)/CLOCKS_PER_SEC);

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);
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

void reinitialize_neighboors (int* neigh_labels, int nodes)
{
    for (int p=0; p<nodes; p++)
        neigh_labels[p] = 0;
}

int get_min_from_array (int* array, int array_size)
{
    int min = array[0];
    for (int t=0; t<array_size; t++)
    {
        if (array[t] == 0)
            break;

        if (array[t] < min) min = array[t];
    }

    return min; // If 0, no neighboors. Error. Should have been start == end.
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

void print_final (int* labels, int iterations)
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
