#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void process_command(const char *command) {

    // this is called for each command from both
    // interactive and shell versions.
    // work on this later

    printf("you entered: %s\n", command);
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
            break;
        }

        command[strcspn(command, "\n")] = '\0';

        if (strcmp(command, "exit") == 0) {
            break;
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
            break;
        }

        process_command(command);
    }

    free(command);
}

int main(int argc, char *argv[]) {
    if (argc == 1) {
        run_shell();
    } else if (argc == 2) {
        FILE *input = fopen(argv[1], "r");
        if (input == NULL) {
            fprintf(stderr, "wish: cannot open file '%s'\n", argv[1]);
            exit(1);
        }
        run_batch(input);
        fclose(input);
    } else {
        fprintf(stderr, "Usage: wish [batch_file]\n");
        exit(1);
    }

    printf("exiting\n");
    return 0;
}