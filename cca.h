#ifndef CCA_H_INCLUDED
#define CCA_H_INCLUDED

void initialize_labels (_Atomic int* labels,int* next_labels, int* active, int nodes);

void initialize_csr_matrix (int* ind_ptr, int* indices);

void reinitialize_matrices (int* next_active, int nodes);

void reinitialize_neighboors (int* neigh_labels, int nodes);

void print_update (int* still_active_labels, int iter);

int get_elements_from_array (int* array, int array_size);

int unique_elements (_Atomic int* labels);

void print_final (_Atomic int* labels, int iterations);

void open_matrix (char* name);

#endif