#ifndef CCA_H_INCLUDED
#define CCA_H_INCLUDED

void initialize_labels (_Atomic int* labels, int* active, int nodes);

void print_update (int iter, int n_active);

int get_elements_from_array (int* array, int array_size);

int unique_elements (_Atomic int* labels);

void print_final (_Atomic int* labels, int iterations);

void open_matrix (char* name);

#endif