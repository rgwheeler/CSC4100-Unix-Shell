#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

// Global State
char **shell_paths = NULL;
int path_count = 0;

// Error handling
void print_error() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message));
}

// Helper to trim whitespace
char *trim_whitespace(char *str) {
    if (!str) return NULL;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Helper to parse space-separated arguments
int parse_args(char *cmd, char **args) {
    int i = 0;
    char *token;
    while ((token = strsep(&cmd, " \t")) != NULL) {
        if (*token != '\0') {
            args[i++] = token;
        }
    }
    args[i] = NULL;
    return i;
}

int main(int argc, char *argv[]) {
    FILE *input = stdin;
    int interactive = 1;

    // Batch vs Interactive Mode
    if (argc == 2) {
        input = fopen(argv[1], "r");
        if (!input) {
            print_error();
            exit(1);
        }
        interactive = 0;
    } else if (argc > 2) {
        print_error();
        exit(1);
    }

    // Initialize default path
    shell_paths = malloc(sizeof(char*) * 1);
    shell_paths[0] = strdup("/bin");
    path_count = 1;

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;

    // Main Loop
    while (1) {
        if (interactive) {
            printf("wish> ");
            fflush(stdout);
        }

        nread = getline(&line, &len, input);
        if (nread == -1) {
            break; // EOF
        }

        if (line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
        }

        char *line_ptr = line;
        char *commands[100];
        int num_cmds = 0;
        char *token;

        // Takes in parallel commands separated by '&'
        while ((token = strsep(&line_ptr, "&")) != NULL) {
            token = trim_whitespace(token);
            if (strlen(token) > 0) {
                commands[num_cmds++] = token;
            }
        }

        pid_t pids[100];
        int pid_count = 0;

        for (int i = 0; i < num_cmds; i++) {
            char *cmd_str = commands[i];

            // Parse for output redirection
            char *cmd_part = strsep(&cmd_str, ">");
            char *file_part = strsep(&cmd_str, ">");

            if (cmd_str != NULL) { 
                print_error(); // Multiple '>' operators
                continue;
            }

            char *out_file = NULL;
            if (file_part != NULL) {
                char *file_args[100];
                int f_count = parse_args(file_part, file_args);
                if (f_count != 1) { 
                    print_error(); // 0 or >1 files right of '>'
                    continue;
                }
                out_file = file_args[0];
            }

            char *args[100];
            int argc_cmd = parse_args(cmd_part, args);

            if (argc_cmd == 0) continue; // Empty command

            // Built-in Commands
            if (strcmp(args[0], "exit") == 0) { // Exit command
                if (argc_cmd > 1) { print_error(); } 
                else { exit(0); }
                continue;
            }

            if (strcmp(args[0], "cd") == 0) { // Change Directory command
                if (argc_cmd != 2) { print_error(); } 
                else if (chdir(args[1]) != 0) { print_error(); }
                continue;
            }

            if (strcmp(args[0], "path") == 0) { // Path command
                for (int p = 0; p < path_count; p++) { free(shell_paths[p]); }
                free(shell_paths);
                
                path_count = argc_cmd - 1;
                if (path_count > 0) {
                    shell_paths = malloc(sizeof(char*) * path_count);
                    for (int p = 0; p < path_count; p++) {
                        shell_paths[p] = strdup(args[p + 1]);
                    }
                } else {
                    shell_paths = NULL;
                }
                continue;
            }

            // Fork for non-builtin commands
            pid_t pid = fork();
            if (pid == 0) {
                // Child Process
                if (out_file != NULL) {
                    int fd = open(out_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) {
                        print_error();
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                // Find executable in paths
                char exec_path[1024];
                int access_ok = 0;

                for (int p = 0; p < path_count; p++) {
                    snprintf(exec_path, sizeof(exec_path), "%s/%s", shell_paths[p], args[0]);
                    if (access(exec_path, X_OK) == 0) {
                        access_ok = 1;
                        break;
                    }
                }

                if (access_ok) {
                    execv(exec_path, args);
                    print_error(); // Execv failed
                    exit(1);
                } else {
                    print_error(); // Command not found
                    exit(1);
                }
            } else if (pid > 0) {
                // Parent Process
                pids[pid_count++] = pid;
            } else {
                print_error(); // Fork failed
            }
        }

        // Wait for all processes in this line to finish
        for (int i = 0; i < pid_count; i++) {
            waitpid(pids[i], NULL, 0);
        }
    }

    // Cleanup
    if (line) free(line);
    for (int p = 0; p < path_count; p++) { free(shell_paths[p]); }
    if (shell_paths) free(shell_paths);
    
    return 0;
}