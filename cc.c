#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <matio.h>
#include <stdatomic.h>
#include "cca.h"
#include <omp.h>

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

    double t0 = wall_time();
    
    int* labels = malloc (n_nodes*sizeof(int));  // label of each node
    int* active = malloc (n_nodes*sizeof(int));  // active nodes
    int* next_active = malloc (n_nodes*sizeof(int)); // nodes that need to be woken up

    int iteration = 0;
    int changed = 1;

    n_active = n_nodes;

    initialize_labels (labels, active, n_nodes);

    while (n_active)
    {
        iteration++;  // Active nodes have stamp == iteration
        int n_active_next = 0;

        #pragma omp parallel for schedule(dynamic, 512) reduction(+:n_active_next)
        for (int i = 0; i < n_nodes; i++)
        {
            if (active[i] != iteration) continue;

            int start = ind_ptr[i];
            int end   = ind_ptr[i+1];
            if (start == end) continue;

            int min_label = labels[indices[start]];
            for (int k = start + 1; k < end; k++)
            {
                int l = labels[indices[k]];
                if (l < min_label)
                    min_label = l;
            }

            if (min_label < labels[i])
            {
                labels[i] = min_label;

                for (int k = start; k < end; k++)
                {
                    int nb = indices[k];

                    if (next_active[nb] != iteration + 1)
                    {
                        next_active[nb] = iteration + 1;
                        n_active_next++;
                    }
                }
            }
        }

        n_active = n_active_next;

        // Swap
        int* temp = active;
        active = next_active;
        next_active = temp;
    }

    free(ind_ptr);
    free(indices);
    free(active);
    free(next_active);
    free(labels);

    double t1 = wall_time();
    printf ("%lf", t1-t0);
    //printf ("Total Connected Components: %d, found in %lf seconds!\n", unique_elements(labels), t1-t0);
    
    return 0;
}

void initialize_labels (int* labels,int* active, int nodes)
{
    #pragma omp parallel for 
    for (int i=0; i<nodes; i++)
    {    
        labels[i] = i+1;
        active[i] = 1;
    }
}

int get_elements_from_array (int* array, int array_size)
{
    int sum = 0;

    #pragma omp parallel for reduction(+:sum)
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

    //printf ("Loaded matrix with %d nodes and %d elements.\n", n_nodes, n_elements);

}
