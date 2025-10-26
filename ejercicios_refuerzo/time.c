#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: time <command> [args...]\n");
        return 1;
    }

    struct timeval start, end;
    if (gettimeofday(&start, NULL) != 0) {
        perror("gettimeofday");
        return 1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    } else if (pid == 0) {
        // Proceso hijo: ejecutar el comando
        execvp(argv[1], &argv[1]);
        // Si execvp retorna, hubo error
        fprintf(stderr, "execvp: %s\n", strerror(errno));
        _exit(127);
    } else {
        int status;
        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return 1;
        }
        if (gettimeofday(&end, NULL) != 0) {
            perror("gettimeofday");
            return 1;
        }
        double elapsed = (end.tv_sec - start.tv_sec)
                       + (end.tv_usec - start.tv_usec) / 1e6;
        printf("Elapsed time: %.5f\n", elapsed);

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            return 128 + WTERMSIG(status);
        }
        return 1;
    }
}