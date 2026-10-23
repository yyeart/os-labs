#define _POSIX_C_SOURCE 200809L

#include "filter.h"

#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

int parse_number(char *text) {
    char *end;

    errno = 0;
    long value = strtol(text, &end, 10);

    if (errno != 0 || end == text || *end != '\0' ||
        value < 0 || value > INT_MAX) {
        return -1;
    }

    return (int)value;
}

int main(int argc, char *argv[]) {
    char *input_file = NULL;
    char *output_file = NULL;

    int window_size = 0;
    int iterations = -1;
    int max_threads = 0;
    int verbose = 0;

    int option;

    while ((option = getopt(argc, argv, "i:o:w:k:t:v")) != -1) {
        switch (option) {
            case 'i':
                input_file = optarg;
                break;

            case 'o':
                output_file = optarg;
                break;
            
            case 'w':
                window_size = parse_number(optarg);
                break;

            case 'k':
                iterations = parse_number(optarg);
                break;

            case 't':
                max_threads = parse_number(optarg);
                break;

            case 'v':
                verbose = 1;
                break;

            default:
            return 1;
        }
    }

    if (input_file == NULL || window_size == 0 ||
        iterations < 0 || max_threads == 0 || optind != argc) {
        fprintf(
            stderr,
            "usage: %s -i input.txt -w W -k K -t P [-o output.txt] [-v]\n",
            argv[0]
        );
        return 1;
    }

    printf("input: %s\n", input_file);

    if (output_file != NULL) {
        printf("output: %s\n", output_file);
    }

    Matrix m = {0};

    if (read_matrix(input_file, &m) != 0) {
        return 1;
    }

    if (apply_filter(&m, window_size, iterations,
                    max_threads, verbose) != 0) {
        free_matrix(&m);
        return 1;
    }

    if (output_file != NULL &&
        write_matrix(output_file, &m) != 0) {
        free_matrix(&m);
        return 1;
    }

    free_matrix(&m);
    return 0;
}