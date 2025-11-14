#ifndef CCA_H_INCLUDED
#define CCA_H_INCLUDED

void initialize_labels (int* labels, int* active, int nodes);

void reinitialize_matrices (int* next_active, int nodes);

void print_update (int* labels, int* still_active_labels, int iter);

int unique_elements (int* labels);

void print_final (int* labels, int iterations);

void open_matrix (char* name);

#endif