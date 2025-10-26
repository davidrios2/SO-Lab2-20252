#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

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

        // Handle parallel commands separated by '&'
        if (strchr(p, '&') != NULL) {
            pid_t pids[128];
            int pcnt = 0;
            char *seg_save = NULL;
            for (char *segment = strtok_r(p, "&", &seg_save); segment != NULL; segment = strtok_r(NULL, "&", &seg_save)) {
                // trim leading/trailing spaces for segment
                while (*segment == ' ' || *segment == '\t') segment++;
                if (*segment == '\0') continue;
                char *end = segment + strlen(segment) - 1;
                while (end >= segment && (*end == ' ' || *end == '\t')) { *end = '\0'; end--; }

                // tokenize segment by whitespace
                char *tokens[128];
                int ntok = 0;
                char *saveptr = NULL;
                for (char *tok = strtok_r(segment, " \t", &saveptr); tok != NULL; tok = strtok_r(NULL, " \t", &saveptr)) {
                    tokens[ntok++] = tok;
                    if (ntok >= 127) break;
                }
                tokens[ntok] = NULL;
                if (ntok == 0) continue;

                // built-ins
                if (strcmp(tokens[0], "exit") == 0) {
                    if (ntok != 1) { print_error(); continue; }
                    free(line);
                    exit(0);
                } else if (strcmp(tokens[0], "cd") == 0) {
                    if (ntok != 2) { print_error(); continue; }
                    if (chdir(tokens[1]) != 0) { print_error(); continue; }
                    continue;
                } else if (strcmp(tokens[0], "path") == 0) {
                    path_count = 0;
                    for (int i = 1; i < ntok && i <= MAX_PATHS; i++) {
                        strncpy(path_list[path_count], tokens[i], MAX_PATH_LEN - 1);
                        path_list[path_count][MAX_PATH_LEN - 1] = '\0';
                        path_count++;
                    }
                    continue;
                }

                // external command with optional redirection '>'
                int redir_idx = -1;
                for (int i = 0; i < ntok; i++) {
                    if (strcmp(tokens[i], ">") == 0) {
                        if (redir_idx != -1) { redir_idx = -2; break; } // multiple '>'
                        redir_idx = i;
                    }
                }
                char *outfile = NULL;
                int args_end = ntok;
                if (redir_idx >= 0) {
                    // '>' must have exactly one filename and be last pair
                    if (redir_idx != ntok - 2) { print_error(); continue; }
                    outfile = tokens[ntok - 1];
                    args_end = redir_idx;
                    if (args_end == 0) { print_error(); continue; }
                } else if (redir_idx == -2) {
                    print_error();
                    continue;
                }

                if (path_count == 0) { print_error(); continue; }

                char candidate[MAX_PATH_LEN * 2];
                int found = 0;
                for (int i = 0; i < path_count; i++) {
                    snprintf(candidate, sizeof(candidate), "%s/%s", path_list[i], tokens[0]);
                    if (access(candidate, X_OK) == 0) { found = 1; break; }
                }
                if (!found) { print_error(); continue; }

                pid_t pid = fork();
                if (pid < 0) { print_error(); continue; }
                if (pid == 0) {
                    // child: set up redirection if requested
                    if (outfile != NULL) {
                        int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                        if (fd < 0) { print_error(); _exit(1); }
                        if (dup2(fd, STDOUT_FILENO) < 0) { print_error(); _exit(1); }
                        if (dup2(fd, STDERR_FILENO) < 0) { print_error(); _exit(1); }
                        close(fd);
                    }
                    char *args[128];
                    int argcnt = 0;
                    args[argcnt++] = tokens[0];
                    for (int j = 1; j < args_end && argcnt < 127; j++) {
                        args[argcnt++] = tokens[j];
                    }
                    args[argcnt] = NULL;
                    execv(candidate, args);
                    print_error();
                    _exit(1);
                } else {
                    // parallel: collect pid
                    if (pcnt < 128) pids[pcnt++] = pid;
                }
            }
            // wait for all children started in this line
            for (int i = 0; i < pcnt; i++) {
                int status;
                waitpid(pids[i], &status, 0);
            }
            continue;
        }

        // No '&' in line: single command path
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
        // Built-in: path
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
        // Unknown/external command with optional redirection
        else {
            // parse redirection
            int redir_idx = -1;
            for (int i = 0; i < ntok; i++) {
                if (strcmp(tokens[i], ">") == 0) {
                    if (redir_idx != -1) { redir_idx = -2; break; }
                    redir_idx = i;
                }
            }
            char *outfile = NULL;
            int args_end = ntok;
            if (redir_idx >= 0) {
                if (redir_idx != ntok - 2) { print_error(); continue; }
                outfile = tokens[ntok - 1];
                args_end = redir_idx;
                if (args_end == 0) { print_error(); continue; }
            } else if (redir_idx == -2) {
                print_error();
                continue;
            }

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
                if (outfile != NULL) {
                    int fd = open(outfile, O_CREAT | O_WRONLY | O_TRUNC, 0666);
                    if (fd < 0) { print_error(); _exit(1); }
                    if (dup2(fd, STDOUT_FILENO) < 0) { print_error(); _exit(1); }
                    if (dup2(fd, STDERR_FILENO) < 0) { print_error(); _exit(1); }
                    close(fd);
                }
                char *args[128];
                int argcnt = 0;
                // argv[0] should be the command name, not full path
                args[argcnt++] = tokens[0];
                for (int j = 1; j < args_end && argcnt < 127; j++) {
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