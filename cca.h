#ifndef CCA_H_INCLUDED
#define CCA_H_INCLUDED

void initialize_labels (_Atomic int* labels,_Atomic int* next_labels, _Atomic int* active, int nodes);

void print_update (_Atomic int* still_active_labels, int iter);

int get_elements_from_array (_Atomic int* array, int array_size);

int unique_elements (_Atomic int* labels);

void print_final (_Atomic int* labels, int iterations);

void open_matrix (char* name);

#endif