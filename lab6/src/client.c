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


struct ClientTask {
  struct Server server;       /* куда подключаться */
  struct FactorialArgs args;  /* что считать: begin, end, mod */
  uint64_t result;            /* сюда поток положит ответ сервера */
  int ok;                     
};

static int connect_to_server(const char *ip, int port) {
  char service[16];

  snprintf(service, sizeof(service), "%d", port);

  struct addrinfo hints = {.ai_family = AF_INET, .ai_socktype = SOCK_STREAM};
  struct addrinfo *res = NULL; /* результат поиска. память выделит getaddrinfo */

  if (getaddrinfo(ip, service, &hints, &res) != 0) {
    return -1;
  }

  int sck = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sck < 0 || connect(sck, res->ai_addr, res->ai_addrlen) != 0) {
    if (sck >= 0) {
      close(sck);
    }
    sck = -1;
  }

  freeaddrinfo(res);
  return sck;
}

static void *ServerWorker(void *arg) {
  struct ClientTask *task = (struct ClientTask *)arg;
  task->ok = 0;

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

  close(sck);
  return NULL;
}

static void usage(const char *prog) {
  fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n", prog);
}

int main(int argc, char **argv) {
  uint64_t k = 0;                    /* что считать: k! */
  uint64_t mod = 0;                  /* по какому модулю */
  const char *servers_path = NULL;   /* путь к файлу серверов */
  bool k_set = false;            

  static struct option options[] = {{"k", required_argument, 0, 'k'},
                                    {"mod", required_argument, 0, 'm'},
                                    {"servers", required_argument, 0, 's'},
                                    {0, 0, 0, 0}}; 


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
      servers_path = optarg;
      break;

    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (!k_set || servers_path == NULL || mod == 0) {
    usage(argv[0]);
    return 1;
  }

  struct Server servers[MAX_SERVERS];
  size_t servers_num = 0; /* сколько серверов уже прочитано */

  FILE *file = fopen(servers_path, "r");
  if (file == NULL) {
    perror("fopen");
    return 1;
  }

  char line[512];
  while (fgets(line, sizeof(line), file) != NULL) {

    char *p = line + strspn(line, " \t");

    /* Пустая строка ('\n'), конец строки ('\0') или комментарий ('#'). */
    if (*p == '\0' || *p == '\n' || *p == '#') {
      continue;
    }

    int port = 0;
    char ip[MAX_IP];

    if (sscanf(p, "%255[^:]:%d", ip, &port) != 2 || port <= 0 ||
        port > 65535) {
      fprintf(stderr, "Bad server line: %s", line);
      continue; 
    }

    if (servers_num == MAX_SERVERS) {
      fprintf(stderr, "Too many servers (max %d)\n", MAX_SERVERS);
      break;
    }

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

    tasks[i] = (struct ClientTask){
        .server = servers[i],
        .args = SplitRange(1, k, (uint64_t)servers_num, i), /* i-я часть 1..k */
        .result = 0,
        .ok = 0,
    };
    tasks[i].args.mod = mod; 

    if (pthread_create(&threads[i], NULL, ServerWorker, &tasks[i]) != 0) {
      fprintf(stderr, "Error: pthread_create failed!\n");

      for (size_t j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
      }

      return 1;
    }
  }

  uint64_t total = 1 % mod;
  bool ok = true; /* стал ли хоть один сервер недоступен */

  for (size_t i = 0; i < servers_num; i++) {

    pthread_join(threads[i], NULL);

    if (!tasks[i].ok) {
      ok = false;
      fprintf(stderr, "Server %s:%d failed\n", tasks[i].server.ip,
              tasks[i].server.port);
    } else {
  
      total = MultModulo(total, tasks[i].result, mod);
    }
  }

  if (ok) {
    printf("answer: %" PRIu64 "\n", total);
  }

  return ok ? 0 : 1; 
}