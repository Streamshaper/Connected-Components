#include <stdio.h>
#include <string.h>
#include <time.h>
#include "cca.h"
#include "cs.h"

// -fdiagnostics-color=always -g -I. cc.c Source/*.c -lm -O3 -o test

int n_nodes = 12;
int n_elements = 16;

int main (int argc, char* argv[])
{
    //========================================//
    FILE *f = fopen ("matrix_fixed.mtx", "r");
    if (!f) 
    {
        perror("Error opening file");
        return 1;
    }
    cs *A = cs_load (f);

    fclose (f);
    if (!A)
    {
        fprintf(stderr, "Error loading matrix.\n");
        return 1;
    }

    cs *A_csc = cs_compress(A);
    cs_spfree(A);

    n_nodes = A_csc->n;
    n_elements = A_csc->p[n_nodes];

    int *ind_ptr = malloc ((n_nodes+1) * sizeof(int)); //row_ptr
    int *indices = malloc (n_elements * sizeof(int));   //col_ind

    memcpy(ind_ptr, A_csc->p, (n_nodes+1)*sizeof(int));
    memcpy(indices, A_csc->i, n_elements*sizeof(int));

    cs_spfree (A_csc);
    
   //initialize_csr_matrix (ind_ptr, indices);

    printf ("You've got %d nodes and %d elements in total.\n", n_nodes, n_elements);

    //========================================//
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
    clock_t check1 = clock();
    clock_t check2;
    initialize_labels (labels, active, n_nodes);
    
    //Saved me: awk '!/^%/ {if (NF==2) print $1, $2, 1; else print $0}' matrix.mtx > matrix_fixed.mtx
    //free the memory afterwards

    while (1)
    {
        iteration++;
        changed = 0;
        reinitialize_matrices(next_active, n_nodes);  // once per iteration, compare with iteration number don't initialize (idea)
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

            if (i%10000 == 0)
            {
                end_t = clock();
                check2 = clock();
                printf ("Iteration completion: %.1f%% || Time elapsed: %.1lf seconds || Speed: %.1lf nodes/second\n", 100.0*i/n_nodes,(double)(end_t-start_t)/CLOCKS_PER_SEC, 
                                                        (double)10000*CLOCKS_PER_SEC/(check2-check1));
                check1 = clock ();
            }
        }

        print_update(labels, next_active, iteration);  // one print per iteration

        if (!changed) break;
        
        int* temp = active;
        active = next_active;
        next_active = temp;

        printf ("UC: %d\n", unique_elements(labels));
    }


    printf ("End, UC: %d", unique_elements(labels));

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
    /*printf("Iteration #%d || Labels: [", iter);
            for (int r=0; r<n_nodes; r++)
            {
                printf ("%d", labels[r]);
                r<n_nodes-1?printf(", "):printf("], ");
            } */
    printf("Iteration: %d ||", iter);
    printf("Still Active: [");
            /*for (int r=0; r<n_nodes; r++)
            {
                printf ("%d", still_active_labels[r]);
                r<n_nodes-1?printf(", "):printf("] ,");
            }*/
    printf("%d nodes. ] ", get_elements_from_array(still_active_labels, n_nodes));
    printf ("\n");
}

int unique_elements (int* labels)
{
    int sum = 0;
    int temp[n_nodes];
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
