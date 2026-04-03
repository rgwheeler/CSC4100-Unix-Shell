#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

char *path[100];
int path_count = 1;

void process_command(const char *command) {
    // --- Handle parallel commands split by '&' ---
    char *cmd_copy = strdup(command);
    char *cmd_ptr = cmd_copy;
    char *segments[100];
    int seg_count = 0;
    char *seg;
    while ((seg = strsep(&cmd_ptr, "&")) != NULL) {
        while (*seg == ' ') seg++;
        if (*seg != '\0') segments[seg_count++] = seg;
    }

    pid_t pids[100];
    int pid_count = 0;

    for (int s = 0; s < seg_count; s++) {
        char *command_copy = strdup(segments[s]);
        char *command_ptr = command_copy;

        char *args[100];
        int arg_count = 0;
        char *redir_file = NULL;
        int redir_count = 0;
        char *token;

        while ((token = strsep(&command_ptr, " ")) != NULL) {
            if (*token == '\0') continue;
            if (strcmp(token, ">") == 0) {
                redir_count++;
                char *file = strsep(&command_ptr, " ");
                while (file && *file == '\0') file = strsep(&command_ptr, " ");
                if (file == NULL) {
                    char error_message[] = "An error has occurred\n";
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    free(command_copy);
                    goto next_seg;
                }
                redir_file = file;
            } else {
                args[arg_count++] = token;
            }
        }
        args[arg_count] = NULL;

        // more than one '>' is an error
        if (redir_count > 1) {
            char error_message[] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            free(command_copy);
            goto next_seg;
        }

        if (arg_count == 0) { free(command_copy); continue; }

        // --- Built-ins ---
        if (strcmp(args[0], "exit") == 0) {
            if (arg_count != 1) {
            }
        }

        if (strcmp(args[0], "cd") == 0) {
            if (arg_count != 2 || chdir(args[1]) != 0) {
                char error_message[] = "An error has occurred\n";
            }
            free(command_copy);
            continue;
        }

        if (strcmp(args[0], "path") == 0) {
            for (int i = 0; i < path_count; i++) free(path[i]);
            path_count = 0;
            for (int i = 1; i < arg_count; i++) {
                path[path_count++] = strdup(args[i]);
            }
            free(command_copy);
            continue;
        }

        // --- Find executable ---
        char full_path[512];
        int found = 0;
        for (int i = 0; i < path_count; i++) {
            snprintf(full_path, sizeof(full_path), "%s/%s", path[i], args[0]);
            if (access(full_path, X_OK) == 0) { found = 1; break; }
        }

        if (!found) {
            char error_message[] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            free(command_copy);
            continue;
        }

        // --- Fork ---
        pid_t pid = fork();
        if (pid < 0) {
            char error_message[] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            free(command_copy);
            continue;
        } else if (pid == 0) {
            if (redir_file != NULL) {
                int fd = open(redir_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) {
                    char error_message[] = "An error has occurred\n";
                    write(STDERR_FILENO, error_message, strlen(error_message));
                    exit(1);
                }
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                close(fd);
            }
            execv(full_path, args);
            char error_message[] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        } else {
            pids[pid_count++] = pid;
        }

        free(command_copy);
        next_seg:;
    }

    // wait for all parallel children
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }

    free(cmd_copy);
}

void run_shell() {
    char *command = NULL;
    size_t len = 0;
    ssize_t bytes_read;

    while (1) {
        printf("wish> ");
        fflush(stdout);

        bytes_read = getline(&command, &len, stdin);
        if (bytes_read == -1) {
            exit(0);
        }

        command[strcspn(command, "\n")] = '\0';

        process_command(command);
    }

    free(command);
}

void run_batch(FILE *input) {
    char *command = NULL;
    size_t len = 0;
    ssize_t bytes_read;

    while ((bytes_read = getline(&command, &len, input)) != -1) {
        command[strcspn(command, "\n")] = '\0';
        process_command(command);
    }

    free(command);
}

int main(int argc, char *argv[]) {
    path[0] = strdup("/bin");

    if (argc == 1) {
        run_shell();
    } else if (argc == 2) {
        FILE *input = fopen(argv[1], "r");
        if (input == NULL) {
            char error_message[] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        run_batch(input);
        fclose(input);
    } else {
        char error_message[] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    return 0;
}              
