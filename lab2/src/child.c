#include "common.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

void reverse_line(char *line, size_t length) {
    if (length < 2) {
        return;
    }

    size_t left = 0;
    size_t right = length - 1;

    while (left < right) {
        char temp = line[left];
        line[left] = line[right];
        line[right] = temp;
        left++;
        right--;
    }
}

int main(void) {
    if (fprintf(stderr, "child: PID=%ld, parent PID=%ld\n",
            (long)getpid(), (long)getppid()) < 0) {
        perror("child: diagnostic output");
        return EXIT_FAILURE;
    }

    char *line = NULL;
    size_t cap = 0;
    int status = EXIT_SUCCESS;

    while (1) {
        errno = 0;
        ssize_t bytes_read = read_line_retry(stdin, &line, &cap);

        if (bytes_read == -1) {
            if (feof(stdin) && !ferror(stdin)) {
                break;
            }

            perror("child: getline");
            status = EXIT_FAILURE;
            break;
        }

        size_t length = (size_t)bytes_read;
        size_t text_length = length;

        if (text_length > 0 && line[text_length - 1] == '\n') {
            text_length--;
        }

        reverse_line(line, text_length);

        if (write_all(STDOUT_FILENO, line, length) == -1) {
            perror("child: write_all");
            status = EXIT_FAILURE;
            break;
        }
    }

    free(line);

    if (fclose(stdin) == EOF) {
        perror("child: close stdin");
        status = EXIT_FAILURE;
    }

    if (fclose(stdout) == EOF) {
        perror("child: close stdout");
        status = EXIT_FAILURE;
    }

    return status;
}