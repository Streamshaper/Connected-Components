#include <stdio.h>
#include "cca.h"

int n_nodes = 12;
int n_edges = 16;

int main (int argc, char* argv[])
{
    int labels [n_nodes];  // ID of each node
    int active [n_nodes];  // Active nodes
    int next_active [n_nodes]; // Nodes that need to be woken up
// Maybe use bool
    int ind_ptr [n_edges];
    int indices [n_edges];
    int neigh_labels [n_nodes]; // n_nodes-1 for no self-loops
    int min_label;
    initialize_labels (labels, active, n_nodes);
    int iteration = 0;
    int changed = 0;
    int start, end;


    initialize_csr_matrix (ind_ptr, indices);

    while (1)
    {
        iteration++;
        changed = 0;
        reinitialize_matrices (next_active, n_nodes);

        for (int i=0; i<n_nodes; i++)
            if (active[i] = 1)
            {
                reinitialize_neighboors (neigh_labels, n_nodes);
                start = ind_ptr[i];
                end = ind_ptr [i+1];

                if (start == end) continue;

                for (int k=start,l=0; k<end; k++, l++)
                    neigh_labels [l] = labels[indices[k]];
                
                min_label = get_min_from_array (neigh_labels, n_nodes);
                
                if (min_label < labels[i])
                {
                    labels[i] = min_label;
                    changed = 1;
                    for (int k=start; k<end; k++)
                        next_active[indices[k]] = 1;
                }                
            }

        print_update (labels, next_active, iteration);
        if (changed == 0) break;
        update_active (active, next_active, n_nodes);
    }

    printf ("UC: %d", unique_elements(labels));
}

void initialize_csr_matrix (int* ind_ptr, int* indices)
{
    int temp_ptr [] = {0, 2, 3, 5, 7, 9, 10, 11, 12, 14, 15, 16, 16};
    for (int q=0; q<n_edges; q++)
        ind_ptr[q] = temp_ptr[q];

    int temp_idx [] = {1, 4, 0, 7, 8, 4, 10, 0, 3, 6, 5, 2, 2, 9, 8, 3};
    for (int q=0; q<n_edges; q++)
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

void update_active (int* active, int* next_active, int nodes)
{
    for (int t=0; t<nodes; t++)
        active[t] = next_active[t];
}

void print_update (int* labels, int* still_active_labels, int iter)
{
    printf("Iteration #%d || Labels: [", iter);
            for (int r=0; r<n_nodes; r++)
            {
                printf ("%d", labels[r]);
                r<n_nodes-1?printf(", "):printf("], ");
            } 
    printf("Still Active: [");
            for (int r=0; r<n_nodes; r++)
            {
                printf ("%d", still_active_labels[r]);
                r<n_nodes-1?printf(", "):printf("] ,");
            }
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
