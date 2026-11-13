/*Что делает программа:
 *   1. Слушает TCP-порт (--port) на всех сетевых интерфейсах.
 *   2. Принимает запросы клиента: begin, end, mod (24 байта).
 *   3. Делит свой диапазон на --tnum частей и считает каждую в отдельном
 *      потоке, затем перемножает результаты по модулю.
 *   4. Отправляет клиенту одно число uint64_t (8 байт).
 *
 * Протокол тот же, что у клиента: 24 байта запроса, 8 байт ответа,
 * порядок байтов как в памяти (host byte order).*/

#include <inttypes.h> 
#include <pthread.h>  
#include <stdint.h>   
#include <stdio.h>    
#include <stdlib.h>  
#include <unistd.h>  

#include <getopt.h>     
#include <netinet/in.h> 
#include <sys/socket.h>
#include <sys/types.h>

#include "utils.h"

#define MAX_TNUM 64

/* Тело потока-факториала */
static void *ThreadFactorial(void *arg) {
  /* Приводим void* к нужному типу и считаем кусок.
   * void* это указатель, а нам нужно вернуть 64-битное число.
   * uintptr_t — целое, в которое гарантированно влезает указатель,
   * поэтому (void *)(uintptr_t)x и обратное преобразование корректны. */
  return (void *)(uintptr_t)Factorial((struct FactorialArgs *)arg);
}

/* Считает (begin * ... * end) % mod на tnum потоках. */
static uint64_t ComputeRange(uint64_t begin, uint64_t end, uint64_t mod,
                             int tnum) {
  if (mod == 0) {
    return 0; 
  }

  /* Массивы фиксированного размера: tnum проверен в main (1..MAX_TNUM). */
  pthread_t threads[MAX_TNUM];
  struct FactorialArgs args[MAX_TNUM];

  for (int i = 0; i < tnum; i++) {
    /* Каждый поток получает свой кусок диапазона. */
    args[i] = SplitRange(begin, end, (uint64_t)tnum, (size_t)i);
    args[i].mod = mod;

    /* &args[i] — адрес i-го элемента; он живёт до pthread_join ниже,
     * поэтому поток успеет прочитать свои данные. */
    if (pthread_create(&threads[i], NULL, ThreadFactorial, &args[i]) != 0) {
      fprintf(stderr, "Error: pthread_create failed!\n");
      exit(1); /* без потоков посчитать нельзя; для лабы это фатально */
    }
  }

  uint64_t total = 1 % mod; /* нейтральный элемент умножения */

  for (int i = 0; i < tnum; i++) {
    void *result = NULL;
    pthread_join(threads[i], &result); /* ждём поток, забираем результат */

    /* (void *)(uintptr_t)result -> обратно в число. */
    total = MultModulo(total, (uint64_t)(uintptr_t)result, mod);
  }

  return total;
}

int main(int argc, char **argv) {
  int port = 0; /* TCP-порт для прослушивания */
  int tnum = 0; /* сколько потоков использовать на запрос */

  /* Длинные опции: --port и --tnum. Возвращаемые значения 'p' и 't'
   * используются только внутри switch, как метки. */
  static struct option options[] = {{"port", required_argument, 0, 'p'},
                                    {"tnum", required_argument, 0, 't'},
                                    {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
    case 'p':
      port = atoi(optarg);
      break;

    case 't':
      tnum = atoi(optarg);
      break;

    default:
      fprintf(stderr, "Unknown argument\n");
      return 1;
    }
  }

  /* Проверка аргументов: корректный порт и разумное число потоков. */
  if (port <= 0 || port > 65535 || tnum <= 0 || tnum > MAX_TNUM) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4 (tnum <= %d)\n", argv[0],
            MAX_TNUM);
    return 1;
  }

  /*Создание "слушающего" сокета
   * AF_INET - IPv4, SOCK_STREAM - TCP, 0 - протокол по умолчанию (TCP). */
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    perror("socket");
    return 1;
  }

  /* Адрес, на котором слушаем.
   * htons/htonl переводят числа в "сетевой порядок байтов" (big-endian):
   * в TCP числа передаются именно так, независимо от процессора.
   * INADDR_ANY = 0.0.0.0 - принимать подключения на всех интерфейсах */
  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons((uint16_t)port),
      .sin_addr.s_addr = htonl(INADDR_ANY),
  };

  /* SO_REUSEADDR позволяет сразу перезапустить сервер на том же порту,
   * не дожидаясь, пока ОС "отпустит" порт после предыдущего запуска. */
  int one = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  /* bind привязывает сокет к адресу и порту. */
  if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("bind");
    return 1;
  }

  /* listen переводит сокет в режим ожидания подключений.
   * SOMAXCONN — максимально допустимая очередь ожидающих клиентов. */
  if (listen(server_fd, SOMAXCONN) < 0) {
    perror("listen");
    return 1;
  }

  printf("Server listening at %d\n", port);

  /* Основной цикл: принимаем клиентов */
  for (;;) {
    /* accept ждёт нового клиента и возвращает НОВЫЙ дескриптор сокета
     * именно для этого соединения. NULL, NULL — адрес клиента нам не нужен. */
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
      perror("accept");
      continue;
    }

    /* Внутренний цикл: по протоколу на одном соединении можно отправить
     * несколько запросов подряд. Клиент из этой лабы отправляет один
     * и закрывает соединение, тогда recv_all вернёт 0 и цикл закончится. */
    struct FactorialArgs request;

    while (recv_all(client_fd, &request, sizeof(request)) == 1) {
      /* Логируем запрос: по этим строкам видно, что сервер получил. */
      fprintf(stdout, "Receive: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
              request.begin, request.end, request.mod);

      /* Считаем диапазон на нескольких потоках. */
      uint64_t total =
          ComputeRange(request.begin, request.end, request.mod, tnum);
      printf("Total: %" PRIu64 "\n", total);

      /* Отправляем 8 байт ответа. Если не получилось, клиент отключился,
       * дальше читать из этого сокета смысла нет. */
      if (send_all(client_fd, &total, sizeof(total)) < 0) {
        break;
      }
    }

    close(client_fd); /* закрываем соединение с этим клиентом */
  }
}