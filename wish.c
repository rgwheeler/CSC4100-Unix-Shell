/*
    Project 1
    CSC 4100
    Jack Siegers, Graham Wheeler
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <limits.h>

char *path[100];
int path_count = 1;

/*
    Standard error message via the rubric
*/
void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

/*
    Checks for valid output redirection
*/
int parse_redirection(char *command, char **outfile) {
    *outfile = NULL;
    int redirection_count = 0;
    char *pos = command;
    char *redirection_pos = NULL;
    while (*pos != '\0') {
        if (*pos == '>') {
            redirection_count++;
            if (redirection_count > 1) {
                return -1;
            }
            redirection_pos = pos;
        }
        pos++;
    }

    if (redirection_count ==0) {
        return 0;
    }

    *redirection_pos = '\0';
    char *file_side  = redirection_pos + 1;

    char *file_token;
    char *file_ptr = file_side;
    int file_count = 0;
    char *found_file = NULL;
    while ((file_token = strsep(&file_ptr, " \t")) != NULL) {
        if (*file_token == '\0') continue;
        if (file_count == 0) {
            *outfile = file_token;
            file_count++;
            found_file = file_token;
        } else {
            return -1;
        }
    }

    if (file_count == 0) {
        return -1;
    }

    *outfile = found_file;
    return 1;
}

/*
    Processes a single command, returns the pid of the child process
*/
pid_t process_command(const char *command) {

    char *command_copy = strdup(command);
    char *outfile = NULL;
    int redirection_result = parse_redirection(command_copy, &outfile);
    if (redirection_result == -1) {
        print_error();
        free(command_copy);
        return 0;
    }

    char *command_ptr = command_copy;
    char *args[100];
    int arg_count = 0;
    char *token;

    while ((token = strsep(&command_ptr, " \t")) != NULL) {
        if (*token =='\0') continue;
        args[arg_count++] = token;
    }
    args[arg_count] = NULL;

    if (arg_count == 0) {
        if (redirection_result == 1) {
            print_error();
        }
        free(command_copy);
        return 0;
    }

    if (strcmp(args[0], "exit") == 0) {
        if (arg_count != 1) {
            print_error();
            free(command_copy);
            return 0;
        } 
        free(command_copy);
        return -1;
    }

    if (strcmp(args[0], "cd") == 0) {
        if (arg_count != 2) {
            print_error();
            free(command_copy);
            return 0;
        }
        if (chdir(args[1]) != 0) {
            print_error();
        }
        free(command_copy);
        return 0;
    }

    if (strcmp(args[0], "path") == 0) {
        for (int i = 0; i < path_count; i++) {
            free(path[i]);
            path[i] = NULL;
        }
        path_count = 0;
        for (int i = 1; i < arg_count; i++) {
            path[path_count++] = strdup(args[i]);
        }
        free(command_copy);
        return 0;
    }

    char full_path[PATH_MAX];
    int found = 0;
    for (int i = 0; i < path_count; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", path[i], args[0]);
        if (access(full_path, X_OK) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        print_error();
        free(command_copy);
        return 0;
    }

    pid_t pid = fork();
    if (pid < 0) {
        print_error();
        free(command_copy);
        return 0;
    } else if (pid == 0) {
        if (outfile != NULL) {
            int fd = open(outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { print_error(); free(command_copy); exit(1); }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
        }
        execv(full_path, args);
        print_error();
        free(command_copy);
        exit(1);
    } else {
        free(command_copy);
        return pid;
    }
    return 0;
    // printf("you entered: %s\n", command);
}

/*
    Wrapper for process_command for multiple commands separated by the user
*/
void run_line(char *line) {
    char *segments[100];
    int count = 0;
    char *line_ptr = line;
    char *segment;
    while ((segment = strsep(&line_ptr, "&")) != NULL) {
        segments[count++] = segment;
    }

    pid_t pids[100];
    int pid_count = 0;
    int should_exit = 0;
    for (int i = 0; i < count; i++) {
        char *seg = segments[i];
        if (strlen(seg) == 0 || strspn(seg, " \t") == strlen(seg)) {
            continue;
        }
        pid_t pid = process_command(segments[i]);
        if (pid == -1) {
            should_exit = 1;
        }
        if (pid > 0) {
            pids[pid_count++] = pid;
        }
    }
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], NULL, 0);
    }
    if (should_exit) {
        exit(0);
    }
}

/*
    Loop for runnning in shell mode
*/
void run_shell() {
    char *command = NULL;
    size_t len = 0;
    ssize_t bytes_read;

    while (1) {
        write(STDOUT_FILENO, "wish> ", 6);

        bytes_read = getline(&command, &len, stdin);
        if (bytes_read == -1) {
            free(command);
            exit(0);
        }

        command[strcspn(command, "\n")] = '\0';

        run_line(command);
    }

    free(command);
}

/*
    Loop for running in batch mode
*/
void run_batch(FILE *input) {
    char *command = NULL;
    size_t len = 0;
    ssize_t bytes_read;

    while ((bytes_read = getline(&command, &len, input)) != -1) {
        command[strcspn(command, "\n")] = '\0';

        run_line(command);
    }
    
    free(command);
}

/*
    Main function that decides interactive or batch modes
*/
int main(int argc, char *argv[]) {
    path[0] = strdup("/bin");

    if (argc == 1) {
        run_shell();
    } else if (argc == 2) {
        FILE *input = fopen(argv[1], "r");
        if (input == NULL) {
            print_error();
            exit(1);
        }
        run_batch(input);
        fclose(input);
    } else {
        print_error();
        exit(1);
    }

    return 0;
}