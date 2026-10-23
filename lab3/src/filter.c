#define _GNU_SOURCE

#include "filter.h"

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct ThreadArgs {
    Matrix *input;
    Matrix *output;
    Matrix *kernel;
    size_t start_row;
    size_t end_row;
    int verbose;
} ThreadArgs;

void filter_once(
    Matrix *input,
    Matrix *output,
    Matrix *kernel,
    size_t start_row,
    size_t end_row
) {
    int window_size = (int)kernel->rows;
    int radius = window_size / 2;

    for (size_t row = start_row; row < end_row; ++row) {
        for (size_t col = 0; col < input->cols; ++col) {
            double sum = 0.0;

            for (int kernel_row = 0; kernel_row < window_size; ++kernel_row) {
                for (int kernel_col = 0; kernel_col < window_size; ++kernel_col) {
                    long long src_row = (long long)row + kernel_row - radius;
                    long long src_col = (long long)col + kernel_col - radius;
                    
                    if (src_row < 0 || src_col < 0 ||
                        src_row >= (long long)input->rows ||
                        src_col >= (long long)input->cols) {
                        continue;
                    }

                    size_t input_index = (size_t)src_row * input->cols + (size_t)src_col;
                    size_t kernel_index = (size_t)kernel_row * kernel->cols + (size_t)kernel_col;

                    sum += input->data[input_index] * kernel->data[kernel_index];
                }
            }

            output->data[row * output->cols + col] = sum;
        }
    }
}

void *filter_thread(void *arg) {
    ThreadArgs *args = arg;

    if (args->verbose) {
        fprintf(
            stderr,
            "PID=%ld TID=%ld rows=[%zu, %zu]\n",
            (long)getpid(),
            (long)gettid(),
            args->start_row,
            args->end_row
        );
    }

    filter_once(
        args->input,
        args->output,
        args->kernel,
        args->start_row,
        args->end_row
    );

    return NULL;
}

int filter_parallel(
    Matrix *input,
    Matrix *output,
    Matrix *kernel,
    int max_threads,
    int verbose
) {
    if (max_threads < 1 || input->rows == 0) {
        fprintf(stderr, "Invalid thread count or matrix size\n");
        return 1;
    }

    size_t count = (size_t)max_threads;
    if (count > input->rows) {
        count = input->rows;
    }

    ThreadArgs *args = (ThreadArgs*)malloc(count * sizeof(ThreadArgs));
    if (!args) {
        fprintf(stderr, "Failed to allocate args\n");
        return 1;
    }

    pthread_t *threads = (pthread_t*)malloc(count * sizeof(pthread_t));
    if (!threads) {
        fprintf(stderr, "Failed to allocate threads\n");
        free(args);
        return 1;
    }

    size_t rows_per_thread = input->rows / count;
    size_t extra_rows = input->rows % count;
    size_t start_row = 0;

    for (size_t i = 0; i < count; ++i) {
        size_t row_count = rows_per_thread;

        if (i < extra_rows) {
            ++row_count;
        }

        args[i] = (ThreadArgs) {
            .input = input,
            .output = output,
            .kernel = kernel,
            .start_row = start_row,
            .end_row = start_row + row_count,
            .verbose = verbose
        };

        start_row += row_count;
    }

    size_t created = 0;
    int failed = 0;

    for (size_t i = 0; i < count - 1; ++i) {
        int err = pthread_create(
            &threads[i], NULL, filter_thread, &args[i]
        );
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            failed = 1;
            break;
        }

        ++created;
    }

    if (!failed) {
        filter_thread(&args[count - 1]);
    }

    for (size_t i = 0; i < created; ++i) {
        int err = pthread_join(threads[i], NULL);
        if (err != 0) {
            fprintf(stderr, "pthread_join: %s\n", strerror(err));
            exit(EXIT_FAILURE);
        }
    }

    free(threads);
    free(args);
    return failed;
}

int apply_filter(
    Matrix *m,
    int window_size,
    int iterations,
    int max_threads,
    int verbose
) {
    if (iterations < 0) {
        fprintf(stderr, "Number of iterations must be non-negative");
        return 1;
    }

    if (iterations == 0) {
        return 0;
    }

    Matrix kernel = {0};
    Matrix output = {0};

    if (create_matrix(&kernel, window_size, window_size) != 0) {
        fprintf(stderr, "Failed to create filter matrix\n");
        return 1;
    }

    if (create_matrix(&output, m->rows, m->cols) != 0) {
        fprintf(stderr, "Failed to create output matrix\n");
        free_matrix(&kernel);
        return 1;
    }

    double coef = 1.0 / (window_size * window_size);

    for (size_t row = 0; row < kernel.rows; ++row) {
        for(size_t col = 0; col < kernel.cols; ++col) {
            kernel.data[row * kernel.cols + col] = coef;
        }
    }

    for (int i = 0; i < iterations; ++i) {
        if (filter_parallel(m, &output, &kernel, max_threads, verbose) != 0) {
            free_matrix(&output);
            free_matrix(&kernel);
            return 1;
        }

        double *tmp = m->data;
        m->data = output.data;
        output.data = tmp;
    }
    
    free_matrix(&output);
    free_matrix(&kernel);
    return 0;
}