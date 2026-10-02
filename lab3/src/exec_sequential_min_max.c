#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // дочерний процесс

        execl("./sequential_min_max",
              "sequential_min_max",
              "123",
              "100",
              NULL);

        // Если exec успешно сработал, сюда выполнение не дойдёт.
        perror("execl");
        return 1;
    }

    // родительский процесс
    if (waitpid(pid, NULL, 0) == -1) {
        perror("waitpid");
        return 1;
    }

    printf("Child process finished\n");

    return 0;
}