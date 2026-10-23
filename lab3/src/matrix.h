#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

typedef struct Matrix {
    size_t rows;
    size_t cols;
    double *data;
} Matrix;

int create_matrix(Matrix *m, size_t rows, size_t cols);
int read_matrix(char *filename, Matrix *m);
int write_matrix(char *filename, Matrix *m);
void free_matrix(Matrix *m);

#endif