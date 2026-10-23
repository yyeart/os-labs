#ifndef FILTER_H
#define FILTER_H

#include "matrix.h"

void filter_once(
    Matrix *input,
    Matrix *output,
    Matrix *kernel,
    size_t row_start,
    size_t row_end
);

int apply_filter(
    Matrix *m,
    int window_size,
    int iterations,
    int max_threads,
    int verbose
);

#endif