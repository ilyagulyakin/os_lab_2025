#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <getopt.h>
#include <pthread.h>


static uint64_t modmul(uint64_t a, uint64_t b, uint64_t mod) {
    if (mod == 0) return 0;
    return (uint64_t)(((__uint128_t)a * b) % mod);
}

/* Аргументы потока */
struct ThreadArg {
    uint64_t begin;  
    uint64_t end;        
    uint64_t mod; /* модуль */
    uint64_t local_prod; /* локальное произведение по модулю */
};

/* Общий результат и мьютекс для его защиты */
static uint64_t g_result = 1;
static pthread_mutex_t g_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Тело потока: считает локальное произведение, затем под мьютексом
 * домножает на него общий результат. */
static void *ThreadFunc(void *arg) {
    struct ThreadArg *t = (struct ThreadArg *)arg;
    uint64_t local = 1 % t->mod;   /* 1 mod mod (для mod=1 даёт 0) */
    for (uint64_t x = t->begin; x <= t->end; x++) {
        local = modmul(local, x % t->mod, t->mod);
        if (local == 0) break;     /* дальше смысла нет — произведение 0 */
    }
    t->local_prod = local;

    /* 2. Синхронизация результата*/
    pthread_mutex_lock(&g_mutex);
    g_result = modmul(g_result, t->local_prod, t->mod);
    pthread_mutex_unlock(&g_mutex);

    return NULL;
}

int main(int argc, char **argv) {
    uint64_t k    = 0;
    uint64_t pnum = 0;
    uint64_t mod  = 0;

    static struct option long_opts[] = {
        {"pnum", required_argument, NULL, 'p'},
        {"mod",  required_argument, NULL, 'm'},
        {NULL, 0, NULL, 0}
    };
    int opt;
    while ((opt = getopt_long(argc, argv, "k:", long_opts, NULL)) != -1) {
        switch (opt) {
            case 'k': k    = strtoull(optarg, NULL, 10); break;
            case 'p': pnum = strtoull(optarg, NULL, 10); break;
            case 'm': mod  = strtoull(optarg, NULL, 10); break;
            default:
                fprintf(stderr,
                    "Usage: %s -k <k> --pnum=<threads> --mod=<mod>\n",
                    argv[0]);
                return 1;
        }
    }
    if (pnum == 0 || mod == 0) {
        fprintf(stderr,
            "Usage: %s -k <k> --pnum=<threads> --mod=<mod>\n",
            argv[0]);
        return 1;
    }

    if (mod == 1) {              /* всё по модулю 1 это 0 */
        printf("Result: 0\n");
        return 0;
    }
    if (k == 0) {                /* 0! = 1 */
        printf("Result: %llu\n", (unsigned long long)(1 % mod));
        return 0;
    }
    if (k >= mod) {
        /* mod делит k!, т.к. k! содержит множитель mod */
        printf("Result: 0\n");
        return 0;
    }

    /*Распределение отрезков между потоками*/
    pthread_t *threads = malloc(sizeof(pthread_t) * pnum);
    struct ThreadArg *targs = malloc(sizeof(struct ThreadArg) * pnum);
    if (!threads || !targs) { perror("malloc"); return 1; }

    uint64_t base = k / pnum;    /* длина «ровной» части */
    uint64_t rem  = k % pnum;    /* остаток первым rem потокам */

    uint64_t cur = 1;
    for (uint64_t i = 0; i < pnum; i++) {
        uint64_t len = base + (i < rem ? 1 : 0);
        if (len == 0) {
            targs[i].begin = 1;
            targs[i].end   = 0;
        } else {
            targs[i].begin = cur;
            targs[i].end   = cur + len - 1;
            cur = targs[i].end + 1;
        }
        targs[i].mod = mod;
        targs[i].local_prod = 1;
    }

    /*Запуск потоков */
    for (uint64_t i = 0; i < pnum; i++) {
        if (pthread_create(&threads[i], NULL, ThreadFunc, &targs[i]) != 0) {
            fprintf(stderr, "Error: pthread_create failed!\n");
            return 1;
        }
    }

    /*Ожидание*/
    for (uint64_t i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Result: %llu\n", (unsigned long long)g_result);

    free(threads);
    free(targs);
    return 0;
}