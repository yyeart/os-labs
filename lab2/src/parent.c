#include "common.h"

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

int close_fd(int *fd) {
    if (*fd == -1) {
        return 0;
    }

    int result = close(*fd);
    *fd = -1;

    if (result == -1) {
        perror("close");
    } 

    return result;
}

int read_filename(char **name, size_t *cap) {
    ssize_t length = read_line_retry(stdin, name, cap);

    if (length == -1) {
        if (feof(stdin) && !ferror(stdin)) {
            fprintf(stderr, "filename not entered\n");
        }
        else {
            perror("getline");
        }

        return -1;
    }

    if (length > 0 && (*name)[length - 1] == '\n') {
        (*name)[length - 1] = '\0';
        length--;
    }

    if (length == 0) {
        fprintf(stderr, "filename cannot be empty");
        return -1;
    }

    return 0;
}

int close_resources(int pipe_fd[2], int *output_fd) {
    int result = 0;

    if (close_fd(&pipe_fd[0]) == -1) {
        result = -1;
    }

    if (close_fd(&pipe_fd[1]) == -1) {
        result = -1;
    }

    if (close_fd(output_fd) == -1) {
        result = -1;
    }

    return result;
}

int wait_for_child(pid_t *pid) {
    if (*pid <= 0) {
        return 0;
    }

    int status;
    pid_t waited_pid;

    do {
        waited_pid = waitpid(*pid, &status, 0);
    } while (waited_pid == -1 && errno == EINTR);

    if (waited_pid == -1) {
        perror("parent: waitpid");
        return -1;
    }

    *pid = -1;

    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);

        if (fprintf(stderr,
                    "child PID=%ld exited with code %d\n",
                    (long)waited_pid,
                    exit_code) < 0) {
            perror("parent: diagnostic output");
            return -1;
        }

        if (exit_code != EXIT_SUCCESS) {
            return -1;
        }

        return 0;
    }

    if (WIFSIGNALED(status)) {
        if (fprintf(stderr,
                    "child PID=%ld terminated by signal %d\n",
                (long)waited_pid,
                WTERMSIG(status)) < 0) {
            perror("parent: diagnostic output");
        }

        return -1;
    }

    if (fprintf(stderr,
                "child PID=%ld: unexpected wait status\n",
                (long)waited_pid) < 0) {
        perror("parent: diagnostic output");
    }

    return -1;
}

int main(void) {
    char *filenames[2] = {NULL, NULL};
    size_t caps[2] = {0, 0};

    int output_files[2] = {-1, -1};
    int result = EXIT_FAILURE;
    
    char *line = NULL;
    size_t line_cap = 0;

    int pipes[2][2] = {
        {-1, -1},
        {-1, -1}
    };
    pid_t child_pids[2] = {-1, -1};

    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;

    if (sigemptyset(&action.sa_mask) == -1) {
        perror("parent: sigemptyset");
        goto cleanup;
    }

    if (sigaction(SIGPIPE, &action, NULL) == -1) {
        perror("parent: sigaction");
        goto cleanup;
    }

    for (int i = 0; i < 2; ++i) {
        fprintf(stderr, "enter filename for child %d:\n", i + 1);

        if (read_filename(&filenames[i], &caps[i]) == -1) {
            goto cleanup;
        }
    }

    for (int i = 0; i < 2; ++i) {
        output_files[i] = open(
            filenames[i],
            O_WRONLY | O_CREAT,
            0644
        );

        if (output_files[i] == -1) {
            perror(filenames[i]);
            goto cleanup;
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (ftruncate(output_files[i], 0) == -1) {
            perror("ftruncate");
            goto cleanup;
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            goto cleanup;
        }
    }

    for (int i = 0; i < 2; ++i) {
        child_pids[i] = fork();

        if (child_pids[i] == -1) {
            perror("fork");
            goto cleanup;
        }

        if (child_pids[i] == 0) {
            if (dup2(pipes[i][0], STDIN_FILENO) == -1) {
                perror("child: dup2 stdin");
                _exit(EXIT_FAILURE);
            }

            if (dup2(output_files[i], STDOUT_FILENO) == -1) {
                perror("child: dup2 stdout");
                _exit(EXIT_FAILURE);
            }

            for (int j = 0; j < 2; ++j) {
                if (close_resources(pipes[j], &output_files[j]) == -1) {
                    _exit(EXIT_FAILURE);
                }
            }
                execl("./child", "child", (char *)NULL);

                perror("child: execl");
                _exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < 2; ++i) {
        if (close_fd(&pipes[i][0]) == -1) {
            goto cleanup;
        }

        if (close_fd(&output_files[i]) == -1) {
            goto cleanup;
        }
    }

    int send_to_first = 1;
    size_t line_number = 1;

    while (1) {
        errno = 0;
        ssize_t bytes_read = read_line_retry(stdin, &line, &line_cap);

        if (bytes_read == -1) {
            if (feof(stdin) && !ferror(stdin)) {
                break;
            }

            perror("parent: getline");
            goto cleanup;
        }

        size_t length = (size_t)bytes_read;

        for (size_t i = 0; i < length; ++i) {
            unsigned char symbol = (unsigned char)line[i];

            if (symbol == '\0' || symbol > 127) {
                fprintf(stderr,
                        "parent: line %zu contains invalid text\n",
                        line_number);
                goto cleanup;
            }
        }

        int child_index;

        if (send_to_first) {
            child_index = 0;
        } else {
            child_index = 1;
        }

        if (fprintf(stderr,
                    "parent PID=%ld: sending line %zu, bytes=%zu "
                    "to child %d, PID=%ld\n",
                    (long)getpid(),
                    line_number,
                    length,
                    child_index+1,
                    (long)child_pids[child_index]) < 0) {
            perror("parent: diagnostic output");
            goto cleanup;
        }

        if (write_all(pipes[child_index][1], line, length) == -1) {
            perror("parent: write_all");
            goto cleanup;
        }

        send_to_first = !send_to_first;
        line_number++;
    }

    result = EXIT_SUCCESS;

cleanup:
    free(line);

    for (int i = 0; i < 2; ++i) {
        if (close_resources(pipes[i], &output_files[i]) == -1) {
            result = EXIT_FAILURE;
        }

        free(filenames[i]);
    }

    for (int i = 0; i < 2; ++i) {
        if (wait_for_child(&child_pids[i]) == -1) {
            result = EXIT_FAILURE;
        }
    }

    return result;
}