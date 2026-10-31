#include <errno.h>
#include <inttypes.h>
#include <netdb.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "utils.h"

struct Server {
  char ip[255];
  int port;
};

struct ClientTask {
  struct Server server;
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
  uint64_t result;
  int ok;
};

static int connect_to_server(const char *ip, int port) {
  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", port);

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *res = NULL;
  int rc = getaddrinfo(ip, port_str, &hints, &res);
  if (rc != 0) {
    return -1;
  }

  int sck = -1;

  for (struct addrinfo *p = res; p != NULL; p = p->ai_next) {
    sck = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sck < 0) {
      continue;
    }

    if (connect(sck, p->ai_addr, p->ai_addrlen) == 0) {
      break;
    }

    close(sck);
    sck = -1;
  }

  freeaddrinfo(res);
  return sck;
}

void *ServerWorker(void *arg) {
  struct ClientTask *task = (struct ClientTask *)arg;
  task->ok = 0;

  int sck = connect_to_server(task->server.ip, task->server.port);
  if (sck < 0) {
    fprintf(stderr, "Connection failed to %s:%d\n", task->server.ip,
            task->server.port);
    return NULL;
  }

  char request[sizeof(uint64_t) * 3];
  memcpy(request, &task->begin, sizeof(uint64_t));
  memcpy(request + sizeof(uint64_t), &task->end, sizeof(uint64_t));
  memcpy(request + 2 * sizeof(uint64_t), &task->mod, sizeof(uint64_t));

  if (send_all(sck, request, sizeof(request)) < 0) {
    fprintf(stderr, "Send failed to %s:%d\n", task->server.ip,
            task->server.port);
    close(sck);
    return NULL;
  }

  char response[sizeof(uint64_t)];
  if (recv_all(sck, response, sizeof(response)) != 1) {
    fprintf(stderr, "Receive failed from %s:%d\n", task->server.ip,
            task->server.port);
    close(sck);
    return NULL;
  }

  memcpy(&task->result, response, sizeof(uint64_t));
  task->ok = 1;

  close(sck);
  return NULL;
}

int main(int argc, char **argv) {
  uint64_t k = 0;
  uint64_t mod = 0;
  bool k_set = false;
  bool mod_set = false;

  char servers_path[255] = {'\0'};

  while (true) {
    static struct option options[] = {{"k", required_argument, 0, 0},
                                      {"mod", required_argument, 0, 0},
                                      {"servers", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        if (!ConvertStringToUI64(optarg, &k)) {
          fprintf(stderr, "Invalid k\n");
          return 1;
        }
        k_set = true;
        break;

      case 1:
        if (!ConvertStringToUI64(optarg, &mod)) {
          fprintf(stderr, "Invalid mod\n");
          return 1;
        }
        mod_set = true;
        break;

      case 2:
        if (strlen(optarg) >= sizeof(servers_path)) {
          fprintf(stderr, "Servers path is too long\n");
          return 1;
        }
        strcpy(servers_path, optarg);
        break;

      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Arguments error\n");
      break;

    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (!k_set || !mod_set || strlen(servers_path) == 0) {
    fprintf(stderr, "Using: %s --k 1000 --mod 5 --servers /path/to/file\n",
            argv[0]);
    return 1;
  }

  if (mod == 0) {
    fprintf(stderr, "mod must be greater than 0\n");
    return 1;
  }

  FILE *file = fopen(servers_path, "r");
  if (file == NULL) {
    perror("fopen");
    return 1;
  }

  struct Server *servers = NULL;
  size_t servers_num = 0;
  size_t servers_cap = 0;

  char line[512];

  while (fgets(line, sizeof(line), file) != NULL) {
    char *p = line;

    while (*p == ' ' || *p == '\t') {
      p++;
    }

    if (*p == '\0' || *p == '\n' || *p == '#') {
      continue;
    }

    char *colon = strchr(p, ':');
    if (colon == NULL) {
      fprintf(stderr, "Bad server line: %s", line);
      continue;
    }

    *colon = '\0';

    char *ip = p;
    char *port_str = colon + 1;

    port_str[strcspn(port_str, "\r\n")] = '\0';

    int port = atoi(port_str);

    if (port <= 0 || port > 65535) {
      fprintf(stderr, "Bad server port: %s\n", port_str);
      continue;
    }

    if (strlen(ip) >= sizeof(servers[0].ip)) {
      fprintf(stderr, "Server IP is too long: %s\n", ip);
      continue;
    }

    if (servers_num == servers_cap) {
      size_t new_cap = servers_cap == 0 ? 4 : servers_cap * 2;
      struct Server *tmp =
          realloc(servers, new_cap * sizeof(struct Server));

      if (tmp == NULL) {
        perror("realloc");
        free(servers);
        fclose(file);
        return 1;
      }

      servers = tmp;
      servers_cap = new_cap;
    }

    memset(&servers[servers_num], 0, sizeof(servers[servers_num]));
    strncpy(servers[servers_num].ip, ip,
            sizeof(servers[servers_num].ip) - 1);
    servers[servers_num].port = port;
    servers_num++;
  }

  fclose(file);

  if (servers_num == 0) {
    fprintf(stderr, "No servers found in %s\n", servers_path);
    free(servers);
    return 1;
  }

  struct ClientTask *tasks = calloc(servers_num, sizeof(struct ClientTask));
  pthread_t *threads = malloc(servers_num * sizeof(pthread_t));

  if (tasks == NULL || threads == NULL) {
    perror("malloc/calloc");
    free(tasks);
    free(threads);
    free(servers);
    return 1;
  }

  uint64_t total_elems = (k >= 1) ? k : 0;
  uint64_t chunk = total_elems / (uint64_t)servers_num;
  uint64_t rem = total_elems % (uint64_t)servers_num;
  uint64_t cur = 1;

  for (size_t i = 0; i < servers_num; i++) {
    tasks[i].server = servers[i];
    tasks[i].mod = mod;

    uint64_t len = chunk + ((uint64_t)i < rem ? 1 : 0);

    if (len == 0) {
      tasks[i].begin = 1;
      tasks[i].end = 0;
    } else {
      tasks[i].begin = cur;
      tasks[i].end = cur + len - 1;
      cur += len;
    }

    if (pthread_create(&threads[i], NULL, ServerWorker, &tasks[i]) != 0) {
      fprintf(stderr, "Error: pthread_create failed!\n");

      for (size_t j = 0; j < i; j++) {
        pthread_join(threads[j], NULL);
      }

      free(tasks);
      free(threads);
      free(servers);
      return 1;
    }
  }

  uint64_t total = 1 % mod;
  bool ok = true;

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

  free(tasks);
  free(threads);
  free(servers);

  return ok ? 0 : 1;
}