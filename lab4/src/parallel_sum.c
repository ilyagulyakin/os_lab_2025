#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_PROCS 1024

static pid_t child_pids[MAX_PROCS];
static int   num_children = 0;

/* Обработчик SIGALRM: убить всех ещё живых детей */
static void on_alarm(int sig) {
    (void)sig;
    for (int i = 0; i < num_children; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGKILL);   /* требование 1 */
        }
    }
}

int main(int argc, char **argv) {
    int timeout = 0;   /* 0 = таймаут не задан */

    /* 1. Разбор --timeout */
    static struct option long_opts[] = {
        {"timeout", required_argument, NULL, 't'},
        {NULL, 0, NULL, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            case 't': timeout = atoi(optarg); break;
            default:
                fprintf(stderr,
                    "Usage: %s [--timeout N] threads_num array_size seed\n",
                    argv[0]);
                return 1;
        }
    }
    if (argc - optind < 3) {
        fprintf(stderr, "Not enough positional arguments\n");
        return 1;
    }
    uint32_t threads_num = (uint32_t)atoi(argv[optind]);
    uint32_t array_size  = (uint32_t)atoi(argv[optind + 1]);
    uint32_t seed        = (uint32_t)atoi(argv[optind + 2]);

    int *array = malloc(sizeof(int) * array_size);
    srand(seed);
    for (uint32_t i = 0; i < array_size; i++)
        array[i] = rand() % 100;

    /* 2. Установка таймаута (только если задан) */
    if (timeout > 0) {
        if (signal(SIGALRM, on_alarm) == SIG_ERR) {   
            perror("signal");
            return 1;
        }
        alarm((unsigned)timeout);                      
    }

    /* 3. Запуск дочерних процессов */
    num_children = (int)threads_num;
    for (uint32_t i = 0; i < threads_num; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            /* Ребёнок: считает сумму на своём отрезке */
            int begin = (int)(i * array_size / threads_num);
            int end   = (int)((i + 1) * array_size / threads_num);
            int sum = 0;
            for (int j = begin; j < end; j++) sum += array[j];
            /* для демонстрации возвращаем младший байт */
            exit(sum & 0xFF);
        } else if (pid > 0) {
            child_pids[i] = pid;
        } else {
            perror("fork");
            return 1;
        }
    }

    /* 4. Неблокирующее ожидание с WNOHANG*/
    int total_sum = 0;
    int remaining = num_children;
    while (remaining > 0) {
        int status;
        pid_t pid = waitpid(-1, &status, WNOHANG);   
        if (pid > 0) {
            remaining--;
            if (WIFEXITED(status))
                total_sum += WEXITSTATUS(status);
            /* WIFSIGNALED -> ребёнок убит по SIGKILL, сумму не добавляем */
        } else if (pid == 0) {
            /* никто пока не завершился - даём обработчику SIGALRM шанс */
            usleep(1000);
        } else { /* pid == -1 */
            if (errno == EINTR) continue;   /* прерваны сигналом */
            break;
        }
    }

    if (timeout > 0) alarm(0);   /* отменить таймер, если всё успели */
    free(array);

    printf("Total: %d\n", total_sum);
    return 0;
}