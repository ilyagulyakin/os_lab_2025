/*Что делает программа:
 *   1. Получает аргументы: --k (что считать), --mod (модуль),
 *      --servers (файл со списком серверов вида "ip:port").
 *   2. Делит диапазон 1..k между серверами (SplitRange).
 *   3. Запускает по одному потоку (pthread) на каждый сервер.
 *      Все потоки работают ОДНОВРЕМЕННО: пока один ждёт ответа,
 *      остальные тоже уже отправили запросы.
 *   4. Дожидается всех (pthread_join) и перемножает частичные ответы
 *      по модулю: k! mod m = (r1 * r2 * ... * rn) mod m.
 *
 * Сетевой протокол (одинаковый у клиента и сервера):
 *   запрос  = struct FactorialArgs: begin, end, mod (3 * 8 = 24 байта);
 *   ответ   = одно число uint64_t (8 байт) — частичный результат.
 *   Порядок байтов — как в памяти (host byte order)*/

#include <inttypes.h>  
#include <netdb.h>     
#include <pthread.h>   
#include <stdbool.h>   
#include <stdint.h>    
#include <stdio.h>     
#include <string.h>    
#include <unistd.h>    

#include <getopt.h>      
#include <netinet/in.h>  
#include <sys/socket.h>  
#include <sys/types.h>   

#include "utils.h" 


#define MAX_SERVERS 64
#define MAX_IP 256 /* максимальная длина строки с IP-адресом */

struct Server {
  char ip[MAX_IP];
  int port;
};

/* Задача для одного потока: что попросить у сервера и куда положить ответ.
 *
 * Каждый поток пишет ТОЛЬКО в свою структуру (tasks[i]), поэтому
 * мьютекс (блокировка) не нужен: гонки за данные нет. Главный поток
 * читает эти поля только после pthread_join, а join гарантирует, что
 * поток завершился и все его записи в память уже видны.
 */
struct ClientTask {
  struct Server server;       /* куда подключаться */
  struct FactorialArgs args;  /* что считать: begin, end, mod */
  uint64_t result;            /* сюда поток положит ответ сервера */
  int ok;                     
};

/* Подключиться к серверу ip:port. Возвращает дескриптор сокета или -1.
 *
 * getaddrinfo - универсальный способ: принимает и "127.0.0.1", и имя хоста
 * ("example.com"), и сам подставляет нужный формат адреса. */
static int connect_to_server(const char *ip, int port) {
  char service[16];
  /* Порт в getaddrinfo передаётся СТРОКОЙ, поэтому число превращаем в текст.
   * snprintf безопаснее sprintf: не выйдет за размер буфера. */
  snprintf(service, sizeof(service), "%d", port);

  /* hints — пожелания к адресу:
   *   AF_INET      — только IPv4 (наш сервер слушает IPv4);
   *   SOCK_STREAM  — TCP */
  struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
  struct addrinfo *res = NULL; /* результат поиска. память выделит getaddrinfo */

  if (getaddrinfo(ip, service, &hints, &res) != 0) {
    return -1; /* адрес не разобрался */
  }

  /* socket() создаёт сокет, connect() устанавливает TCP-соединение. */
  int sck = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sck < 0 || connect(sck, res->ai_addr, res->ai_addrlen) != 0) {
    if (sck >= 0) {
      close(sck);
    }
    sck = -1;
  }

  freeaddrinfo(res); /* память выделял getaddrinfo, освобождаем её сами */
  return sck;
}

/* Тело потока. arg - указатель на нашу struct ClientTask (передаём &tasks[i]). */
static void *ServerWorker(void *arg) {
  /* Приводим void* обратно к конкретному типу*/
  struct ClientTask *task = (struct ClientTask *)arg;
  task->ok = 0; /* по умолчанию считаем, что не получилось */

  int sck = connect_to_server(task->server.ip, task->server.port);
  if (sck < 0) {
    return NULL;
  }

  /* Отправляем 24 байта запроса и принимаем 8 байт ответа.
   * send_all/recv_all возвращают успех/неуспех */
  if (send_all(sck, &task->args, sizeof(task->args)) == 0 &&
      recv_all(sck, &task->result, sizeof(task->result)) == 1) {
    task->ok = 1;
  }

  close(sck); /* соединение больше не нужно */
  return NULL;
}

/* Подсказка при неверных аргументах. prog - имя программы (argv[0]). */
static void usage(const char *prog) {
  fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", prog);
}

int main(int argc, char **argv) {
  uint64_t k = 0;                    /* что считать: k! */
  uint64_t mod = 0;                  /* по какому модулю */
  const char *servers_path = NULL;   /* путь к файлу серверов */
  bool k_set = false;                /* задан ли --k (k = 0 — допустимо) */

  /* Описание длинных опций для getopt_long.
   * Поля: имя, обязателен ли аргумент, куда писать флаг (0 — не нужно),
   * и какое значение вернуть из getopt_long. Мы возвращаем буквы,
   * чтобы switch ниже был простым. */
  static struct option options[] = {{"k", required_argument, 0, 'k'},
                                    {"mod", required_argument, 0, 'm'},
                                    {"servers", required_argument, 0, 's'},
                                    {0, 0, 0, 0}}; /* завершающий элемент */

  /* getopt_long разбирает аргументы по одному, пока не вернёт -1.
   * optarg - указатель на значение опции (например, "1000"). */
  int c;
  while ((c = getopt_long(argc, argv, "", options, NULL)) != -1) {
    switch (c) {
    case 'k':
      if (!ConvertStringToUI64(optarg, &k)) {
        fprintf(stderr, "Invalid k\n");
        return 1;
      }
      k_set = true;
      break;

    case 'm':
      if (!ConvertStringToUI64(optarg, &mod)) {
        fprintf(stderr, "Invalid mod\n");
        return 1;
      }
      break;

    case 's':
      /* optarg указывает внутрь argv, копировать не нужно: argv живёт
       * до конца программы. */
      servers_path = optarg;
      break;

    default: /* неизвестная опция или не хватает аргумента */
      usage(argv[0]);
      return 1;
    }
  }

  /* k == 0 допустим: 0! == 1. А вот mod == 0 - деление на ноль, запрещаем. */
  if (!k_set || servers_path == NULL || mod == 0) {
    usage(argv[0]);
    return 1;
  }

  /* Чтение файла серверов 
   * Формат файла: по одной строке "ip:port" на сервер.
   * Пустые строки и строки, начинающиеся с '#', игнорируются. */
  struct Server servers[MAX_SERVERS];
  size_t servers_num = 0; /* сколько серверов уже прочитано */

  FILE *file = fopen(servers_path, "r"); /* "r" - только чтение */
  if (file == NULL) {
    perror("fopen");
    return 1;
  }

  char line[512];
  /* fgets читает одну строку (до 511 символов) и возвращает NULL в конце. */
  while (fgets(line, sizeof(line), file) != NULL) {
    /* strspn возвращает длину начала строки из символов " \t".
     * Так мы пропускаем ведущие пробелы и табуляции. */
    char *p = line + strspn(line, " \t");

    /* Пустая строка ('\n'), конец строки ('\0') или комментарий ('#'). */
    if (*p == '\0' || *p == '\n' || *p == '#') {
      continue;
    }

    int port = 0;
    char ip[MAX_IP];

    /* sscanf разбирает строку по шаблону:
     *   %255[^:]  - до 255 символов, кроме ':' (это IP; 255 — чтобы
     *               гарантированно осталось место под '\0' в char[256]);
     *   :         - буквальное двоеточие;
     *   %d        - целое число (порт).
     * Функция возвращает количество успешно разобранных полей: нужно 2. */
    if (sscanf(p, "%255[^:]:%d", ip, &port) != 2 || port <= 0 ||
        port > 65535) {
      fprintf(stderr, "Bad server line: %s", line);
      continue; 
    }

    if (servers_num == MAX_SERVERS) {
      fprintf(stderr, "Too many servers (max %d)\n", MAX_SERVERS);
      break;
    }

    /* snprintf(copy) - безопасное копирование строки с ограничением длины. */
    snprintf(servers[servers_num].ip, MAX_IP, "%s", ip);
    servers[servers_num].port = port;
    servers_num++;
  }

  fclose(file);

  if (servers_num == 0) {
    fprintf(stderr, "No servers found in %s\n", servers_path);
    return 1;
  }

  /*Запуск потоков
   * tasks[i] - данные для i-го потока, threads[i] - его идентификатор. */
  struct ClientTask tasks[MAX_SERVERS];
  pthread_t threads[MAX_SERVERS];

  for (size_t i = 0; i < servers_num; i++) {
    /* "Составной литерал" (compound literal): создаём структуру и сразу
     * заполняем поля по именам. Так видно, что чему присваивается. */
    tasks[i] = (struct ClientTask){
        .server = servers[i], /* копия структуры (массив char копируется) */
        .args = SplitRange(1, k, (uint64_t)servers_num, i), /* i-я часть 1..k */
        .result = 0,
        .ok = 0,
    };
    tasks[i].args.mod = mod; 

    if (pthread_create(&threads[i], NULL, ServerWorker, &tasks[i]) != 0) {
      fprintf(stderr, "Error: pthread_create failed!\n");

      /* Уже запущенные потоки нельзя оставлять "висеть": дождёмся их,
       * иначе память tasks/threads исчезнет из-под них при выходе. */
      for (size_t j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
      }

      return 1;
    }
  }

  /* Сбор результатов
   * 1 % mod - нейтральный элемент: с него начинаем умножение. */
  uint64_t total = 1 % mod;
  bool ok = true; /* стал ли хоть один сервер недоступен */

  for (size_t i = 0; i < servers_num; i++) {
    /* join ждёт завершения потока. После него поля tasks[i] можно
     * безопасно читать: поток уже всё записал. */
    pthread_join(threads[i], NULL);

    if (!tasks[i].ok) {
      ok = false;
      fprintf(stderr, "Server %s:%d failed\n", tasks[i].server.ip,
              tasks[i].server.port);
    } else {
      /* Перемножаем частичные результаты по модулю:
       * k! mod m = (r1 * r2 * ... * rn) mod m. */
      total = MultModulo(total, tasks[i].result, mod);
    }
  }

  if (ok) {
    printf("answer: %" PRIu64 "\n", total);
  }

  return ok ? 0 : 1; 
}