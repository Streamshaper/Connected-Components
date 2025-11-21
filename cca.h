#ifndef CCA_H_INCLUDED
#define CCA_H_INCLUDED

void initialize_labels (int* labels, int* active, int* next_active, int nodes);

void print_update (int iter, int n_active);

int get_elements_from_array (int* array, int array_size);

int unique_elements (int* labels);

void open_matrix (char* name);

double wall_time();

void* worker(void* arg);

#endif