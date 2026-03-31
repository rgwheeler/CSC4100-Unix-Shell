#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

char *path[100];
int path_count = 1;

void process_command(const char *command) {

    // this is called for each command from both
    // interactive and shell versions.
    // work on this later

    char *command_copy = strdup(command);
    char *command_ptr = command_copy;

    char *args[100];
    int arg_count = 0;
    char *token;
    while ((token = strsep(&command_ptr, " ")) != NULL) {
        if (*token =='\0') continue;
        args[arg_count++] = token;
    }
    args[arg_count] = NULL;

    if (arg_count == 0) {
        free(command_copy);
        return;
    }

    char full_path[200];
    int found = 0;
    for (int i = 0; i < path_count; i++) {
        snprintf(full_path, sizeof(full_path), "%s/%s", path[i], args[0]);
        if (access(full_path, X_OK) == 0) {
            found = 1;
            break;
        }
    }

    if (!found) {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        free(command_copy);
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        free(command_copy);
        return;
    } else if (pid == 0) {
        execv(full_path, args);
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    } else {
        waitpid(pid, NULL, 0);
    }
    
    free(command_copy);

    // printf("you entered: %s\n", command);
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

        if (strcmp(command, "exit") == 0) {
            exit(0);
        }

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

        if (strcmp(command, "exit") == 0) {
            exit(0);
        }

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
            char error_message[30] = "An error has occurred\n";
            write(STDERR_FILENO, error_message, strlen(error_message));
            exit(1);
        }
        run_batch(input);
        fclose(input);
    } else {
        char error_message[30] = "An error has occurred\n";
        write(STDERR_FILENO, error_message, strlen(error_message));
        exit(1);
    }

    printf("exiting\n");
    return 0;
}