
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <time.h>
#include <pthread.h>

#include "sum_lib.h"

/* GenerateArray из лабораторной №3 (адаптирована под int) */
static void GenerateArray(int *array, unsigned int array_size, unsigned int seed) {
    srand(seed);
    for (unsigned int i = 0; i < array_size; i++) {
        array[i] = rand() % 100;
    }
}

/* Аргумент для потока: отрезок + куда положить результат */
struct ThreadArg {
    struct SumArgs args;
    int result;
};

static void *ThreadSum(void *arg) {
    struct ThreadArg *t = (struct ThreadArg *)arg;
    t->result = Sum(&t->args);
    return NULL;
}

int main(int argc, char **argv) {
    uint32_t threads_num = 0;
    uint32_t array_size  = 0;
    uint32_t seed        = 0;

    /*Разбор именных аргументов*/
    static struct option long_opts[] = {
        {"threads_num", required_argument, NULL, 't'},
        {"array_size",  required_argument, NULL, 'a'},
        {"seed",        required_argument, NULL, 's'},
        {NULL, 0, NULL, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "", long_opts, NULL)) != -1) {
        switch (opt) {
            case 't': threads_num = (uint32_t)atoi(optarg); break;
            case 'a': array_size  = (uint32_t)atoi(optarg); break;
            case 's': seed        = (uint32_t)atoi(optarg); break;
            default:
                fprintf(stderr,
                    "Usage: %s --threads_num N --array_size N --seed N\n",
                    argv[0]);
                return 1;
        }
    }
    if (threads_num == 0 || array_size == 0) {
        fprintf(stderr,
            "Usage: %s --threads_num N --array_size N --seed N\n",
            argv[0]);
        return 1;
    }

    /*Генерация массива (ВНЕ замера времени)*/
    int *array = malloc(sizeof(int) * array_size);
    if (!array) { perror("malloc"); return 1; }
    GenerateArray(array, array_size, seed);

    /*Подготовка потоков */
    pthread_t *threads = malloc(sizeof(pthread_t) * threads_num);
    struct ThreadArg *targs = malloc(sizeof(struct ThreadArg) * threads_num);
    if (!threads || !targs) { perror("malloc"); return 1; }

    for (uint32_t i = 0; i < threads_num; i++) {
        targs[i].args.array = array;
        targs[i].args.begin = (int)(i * array_size / threads_num);
        targs[i].args.end   = (int)((i + 1) * array_size / threads_num);
        targs[i].result = 0;
    }

    /*Замер времени*/
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (uint32_t i = 0; i < threads_num; i++) {
        if (pthread_create(&threads[i], NULL, ThreadSum, &targs[i]) != 0) {
            fprintf(stderr, "Error: pthread_create failed!\n");
            return 1;
        }
    }

    int total_sum = 0;
    for (uint32_t i = 0; i < threads_num; i++) {
        pthread_join(threads[i], NULL);
        total_sum += targs[i].result;
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);

    double elapsed = (double)(t1.tv_sec - t0.tv_sec)
                   + (double)(t1.tv_nsec - t0.tv_nsec) / 1e9;

    printf("Total: %d\n", total_sum);
    printf("Time:  %.6f sec\n", elapsed);

    free(array);
    free(threads);
    free(targs);
    return 0;
}