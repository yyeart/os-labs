#include "matrix.h"

#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

int create_matrix(Matrix *m, size_t rows, size_t cols) {
    if (rows == 0 || cols == 0) {
        return 1;
    }

    if (!m) {
        return 1;
    }

    m->data = (double*)malloc(rows * cols * sizeof(double));
    if (!m->data) {
        return 1;
    }

    m->rows = rows;
    m->cols = cols;

    return 0;
}

void free_matrix(Matrix *m) {
    free(m->data);
    m->data = NULL;
    m->cols = 0;
    m->rows = 0;
}

int read_matrix(char* filename, Matrix *m) {
    if (filename == NULL || m == NULL || m->data != NULL) {
        fprintf(stderr, "read_matrix requires an empty matrix\n");
        return 1;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        perror(filename);
        return 1;
    }

    size_t rows = 0;
    size_t cols = 0;
    int result = 1;

    if (fscanf(file, "%zu %zu", &rows, &cols) != 2) {
        fprintf(stderr, "failed to read matrix dimensions\n");
    } else {
        result = 0;

        if (create_matrix(m, rows, cols) != 0) {
            fprintf(stderr, "failed to create matrix\n");
            result = 1;
        }


        if (result == 0) {
            for (size_t i = 0; i < rows * cols; ++i) {
                if (fscanf(file, "%lf", &m->data[i]) != 1) {
                    fprintf(stderr, "missing element %zu\n", i);
                    result = 1;
                    break;
                }
            }
        }

        if (result == 0) {
            char extra;

            if (fscanf(file, " %c", &extra) != EOF || ferror(file)) {
                fprintf(stderr, "extra data or read error\n");
                result = 1;
            }
        }
    }

    if (fclose(file) != 0) {
        perror(filename);
        result = 1;
    }

    if (result != 0) {
        free_matrix(m);
    }

    return result;
}

int write_matrix(char *filename, Matrix *m) {
    if (filename == NULL || m == NULL || m->data == NULL ||
        m->rows == 0 || m->cols == 0) {
        fprintf(stderr, "invalid matrix for writing\n");
        return 1;
    }

    FILE *file = fopen(filename, "w");
    if(!file) {
        perror(filename);
        return 1;
    }

    int result = 0;

    if (fprintf(file, "%zu %zu\n", m->rows, m->cols) < 0) {
        result = 1;
    }

    for (size_t row = 0; row < m->rows && result == 0; ++row) {
        for (size_t col = 0; col < m->cols; ++col) {
            size_t index = row * m->cols + col;

            if (fprintf(file, "%.17g ", m->data[index]) < 0) {
                result = 1;
                break;
            }
        }

        if (result == 0 && fprintf(file, "\n") < 0) {
            result = 1;
        }
    }

    if (result != 0) {
        perror(filename);
    }

    if (fclose(file) != 0) {
        perror(filename);
        result = 1;
    }

    return result;
}