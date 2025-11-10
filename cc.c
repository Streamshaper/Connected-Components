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
    
    printf ("You've got %d nodes and %d elements in total.\n", n_nodes, n_elements);

    //========================================//
    int labels [n_nodes];  // label of each node
    int active [n_nodes];  // active nodes
    int next_active [n_nodes]; // nodes that need to be woken up
    int neigh_labels [n_nodes]; // neighbours' labels for a given node
    int min_label, start, end;
    int n_neigh = 0;
    int iteration = 0;
    int changed = 0;

    clock_t start_t;
    clock_t end_t;
    clock_t iter_t;
    clock_t rate_t = clock();
    initialize_labels (labels, active, n_nodes);
    //initialize_csr_matrix (ind_ptr, indices);

    //Saved me: awk '!/^%/ {if (NF==2) print $1, $2, 1; else print $0}' matrix.mtx > matrix_fixed.mtx
    //free the memory afterwards

    while (1)
    {
        iteration++;
        changed = 0;
        reinitialize_matrices(next_active, n_nodes);  // once per iteration, compare with iteration number don't initialize (idea)
        rate_t = clock();
        for (int i=0; i<n_nodes; i++)
        {
            iter_t = 0;
            start_t = clock();
            if (active[i] != 1) continue;

            start = ind_ptr[i];
            end   = ind_ptr[i+1];
            if (start == end) continue;
            end_t = clock();
            iter_t += (end_t-start_t);
            //if (i%10000 == 0) printf ("Ttstart: %lf || ", (double)(end_t-start_t)/CLOCKS_PER_SEC);

            min_label = labels[indices[start]];

            for (int k = start+1; k < end; k++)
                if (labels[indices[k]] < min_label)
                    min_label = labels[indices[k]];
            
            start_t = clock();
            iter_t += (start_t-end_t);
            //if (i%10000 == 0) printf ("Ttmin: %lf || ", (double)(start_t-end_t)/CLOCKS_PER_SEC);

            if (min_label < labels[i])
            {
                labels[i] = min_label;
                changed = 1;
                for (int k=start; k<end; k++)
                    next_active[indices[k]] = 1;
            }

            end_t = clock();
            iter_t += (end_t-start_t);
            if (i%5000 == 0)
            {
                printf ("Ttupdate: %lf || Ttotal: %lf || %.1f%% || Rate %lfs for 5000 nodes.\n", (double)(end_t-start_t)/CLOCKS_PER_SEC, 
                                        (double)(iter_t)/CLOCKS_PER_SEC, 100.0*i/n_nodes,
                                        (double)(clock()-rate_t)/CLOCKS_PER_SEC);
                rate_t = clock();
            }
        }

        print_update(labels, next_active, iteration);  // one print per iteration

        if (!changed) break;
        update_active(active, next_active, n_nodes);
        printf ("UC: %d\n", unique_elements(labels));
    }


    printf ("UC: %d", unique_elements(labels));

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

    printf("You know, it's %d", sum);
    return sum;
}

void update_active (int* active, int* next_active, int nodes)
{
    /*for (int t=0; t<nodes; t++)
        active[t] = next_active[t];*/

    int* temp = active;
    active = next_active;
    next_active = temp;
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
