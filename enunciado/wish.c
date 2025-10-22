#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
//#include <errno.h>

static const char error_message[] = "An error has occurred\n";

static void print_error(void) {
    write(STDERR_FILENO, error_message, strlen(error_message));
}

#define MAX_PATHS 64
#define MAX_PATH_LEN 256
static char path_list[MAX_PATHS][MAX_PATH_LEN];
static int path_count = 0;

int main(int argc, char *argv[]) {
    FILE *input = NULL;

    // initialize default search path
    strncpy(path_list[0], "/bin", MAX_PATH_LEN - 1);
    path_list[0][MAX_PATH_LEN - 1] = '\0';
    strncpy(path_list[1], "/usr/bin", MAX_PATH_LEN - 1);
    path_list[1][MAX_PATH_LEN - 1] = '\0';
    path_count = 2;

    if (argc == 1) {
        input = stdin;
    } else if (argc == 2) {
        input = fopen(argv[1], "r");
        if (input == NULL) {
            print_error();
            exit(1);
        }
    } else {
        print_error();
        exit(1);
    }

    char *line = NULL;
    size_t cap = 0;

    while (1) {
        if (argc == 1) {
            // Interactive mode prints prompt
            printf("wish> ");
            fflush(stdout);
        }

        ssize_t nread = getline(&line, &cap, input);
        if (nread == -1) {
            // EOF
            free(line);
            exit(0);
        }

        // Remove trailing newline and carriage return
        if (nread > 0 && line[nread - 1] == '\n') {
            line[nread - 1] = '\0';
            nread--;
        }
        if (nread > 0 && line[nread - 1] == '\r') {
            line[nread - 1] = '\0';
        }

        // Skip leading whitespace
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') {
            // Empty line
            continue;
        }

        // Tokenize by whitespace
        char *tokens[128];
        int ntok = 0;
        char *saveptr = NULL;
        for (char *tok = strtok_r(p, " \t", &saveptr); tok != NULL; tok = strtok_r(NULL, " \t", &saveptr)) {
            tokens[ntok++] = tok;
            if (ntok >= 127) break;
        }
        tokens[ntok] = NULL;
        if (ntok == 0) {
            continue;
        }

        // Built-in: exit
        if (strcmp(tokens[0], "exit") == 0) {
            if (ntok != 1) {
                print_error();
                continue;
            }
            free(line);
            exit(0);
        }
        // Built-in: cd
        else if (strcmp(tokens[0], "cd") == 0) {
            if (ntok != 2) {
                print_error();
                continue;
            }
            if (chdir(tokens[1]) != 0) {
                print_error();
                continue;
            }
        }
        // Built-in: path (placeholder; full behavior later)
        else if (strcmp(tokens[0], "path") == 0) {
            // Set shell search path to provided arguments
            path_count = 0;
            for (int i = 1; i < ntok && i <= MAX_PATHS; i++) {
                strncpy(path_list[path_count], tokens[i], MAX_PATH_LEN - 1);
                path_list[path_count][MAX_PATH_LEN - 1] = '\0';
                path_count++;
            }
            continue;
        }
        // Unknown/external command: not implemented yet -> error
        else {
            // external command execution using path
            if (path_count == 0) {
                print_error();
                continue;
            }
            char candidate[MAX_PATH_LEN * 2];
            int found = 0;
            for (int i = 0; i < path_count; i++) {
                snprintf(candidate, sizeof(candidate), "%s/%s", path_list[i], tokens[0]);
                if (access(candidate, X_OK) == 0) {
                    found = 1;
                    break;
                }
            }
            if (!found) {
                print_error();
                continue;
            }
            pid_t pid = fork();
            if (pid < 0) {
                print_error();
                continue;
            } else if (pid == 0) {
                char *args[128];
                int argcnt = 0;
                // argv[0] should be the command name, not full path
                args[argcnt++] = tokens[0];
                for (int j = 1; j < ntok && argcnt < 127; j++) {
                    args[argcnt++] = tokens[j];
                }
                args[argcnt] = NULL;
                execv(candidate, args);
                // if execv returns, it's an error
                print_error();
                _exit(1);
            } else {
                // parent waits for child to finish
                int status;
                waitpid(pid, &status, 0);
            }
            continue;
        }
    }
}