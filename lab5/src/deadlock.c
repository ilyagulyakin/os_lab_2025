#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t m1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t m2 = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *arg) {
    (void)arg;
    pthread_mutex_lock(&m1);
    printf("T1: взял m1\n"); fflush(stdout);
    sleep(1);
    printf("T1: жду m2...\n"); fflush(stdout);
    pthread_mutex_lock(&m2);      /*ждёт вечно */
    printf("T1: взял m2\n");
    pthread_mutex_unlock(&m2);
    pthread_mutex_unlock(&m1);
    return NULL;
}

void *thread2(void *arg) {
    (void)arg;
    pthread_mutex_lock(&m2);
    printf("T2: взял m2\n"); fflush(stdout);
    sleep(1);
    printf("T2: жду m1...\n"); fflush(stdout);
    pthread_mutex_lock(&m1);      /*ждёт вечно */
    printf("T2: взял m1\n");
    pthread_mutex_unlock(&m1);
    pthread_mutex_unlock(&m2);
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, thread1, NULL);
    pthread_create(&t2, NULL, thread2, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Готово\n");    /* никогда не выполнится */
    return 0;
}