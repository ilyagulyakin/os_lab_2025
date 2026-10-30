#include <inttypes.h>
#include <limits.h>
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

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
};

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
  if (mod == 0) {
    return 0;
  }

  uint64_t result = 0;
  a = a % mod;
  while (b > 0) {
    if (b % 2 == 1)
      result = (result + a) % mod;
    a = (a * 2) % mod;
    b /= 2;
  }

  return result % mod;
}

uint64_t Factorial(const struct FactorialArgs *args) {
  if (args->mod == 0) {
    return 0;
  }

  uint64_t ans = 1 % args->mod;

  if (args->begin > args->end) {
    return ans;
  }

  for (uint64_t i = args->begin;; ++i) {
    ans = MultModulo(ans, i, args->mod);
    if (i == args->end) {
      break;
    }
  }

  return ans;
}

void *ThreadFactorial(void *args) {
  struct FactorialArgs *fargs = (struct FactorialArgs *)args;
  return (void *)(uintptr_t)Factorial(fargs);
}

static int recv_all(int fd, void *buf, size_t len) {
  char *p = (char *)buf;
  size_t off = 0;

  while (off < len) {
    ssize_t n = recv(fd, p + off, len - off, 0);
    if (n == 0) {
      return 0;
    }
    if (n < 0) {
      return -1;
    }
    off += (size_t)n;
  }

  return 1;
}

static int send_all(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  size_t off = 0;

  while (off < len) {
    ssize_t n = send(fd, p + off, len - off, 0);
    if (n <= 0) {
      return -1;
    }
    off += (size_t)n;
  }

  return 0;
}

int main(int argc, char **argv) {
  int tnum = -1;
  int port = -1;

  while (true) {
    static struct option options[] = {{"port", required_argument, 0, 0},
                                      {"tnum", required_argument, 0, 0},
                                      {0, 0, 0, 0}};

    int option_index = 0;
    int c = getopt_long(argc, argv, "", options, &option_index);

    if (c == -1)
      break;

    switch (c) {
    case 0: {
      switch (option_index) {
      case 0:
        port = atoi(optarg);
        if (port <= 0 || port > 65535) {
          fprintf(stderr, "Invalid port\n");
          return 1;
        }
        break;

      case 1:
        tnum = atoi(optarg);
        if (tnum <= 0) {
          fprintf(stderr, "Invalid tnum\n");
          return 1;
        }
        break;

      default:
        printf("Index %d is out of options\n", option_index);
      }
    } break;

    case '?':
      printf("Unknown argument\n");
      break;

    default:
      fprintf(stderr, "getopt returned character code 0%o?\n", c);
    }
  }

  if (port == -1 || tnum == -1) {
    fprintf(stderr, "Using: %s --port 20001 --tnum 4\n", argv[0]);
    return 1;
  }

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    fprintf(stderr, "Can not create server socket!\n");
    return 1;
  }

  struct sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons((uint16_t)port);
  server.sin_addr.s_addr = htonl(INADDR_ANY);

  int opt_val = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));

  int err = bind(server_fd, (struct sockaddr *)&server, sizeof(server));
  if (err < 0) {
    fprintf(stderr, "Can not bind to socket!\n");
    return 1;
  }

  err = listen(server_fd, 128);
  if (err < 0) {
    fprintf(stderr, "Could not listen on socket\n");
    return 1;
  }

  printf("Server listening at %d\n", port);

  while (true) {
    struct sockaddr_in client;
    socklen_t client_len = sizeof(client);
    int client_fd = accept(server_fd, (struct sockaddr *)&client, &client_len);

    if (client_fd < 0) {
      fprintf(stderr, "Could not establish new connection\n");
      continue;
    }

    while (true) {
      char from_client[sizeof(uint64_t) * 3];

      int read_result = recv_all(client_fd, from_client, sizeof(from_client));

      if (read_result == 0) {
        break;
      }
      if (read_result < 0) {
        fprintf(stderr, "Client read failed\n");
        break;
      }

      uint64_t begin = 0;
      uint64_t end = 0;
      uint64_t mod = 0;

      memcpy(&begin, from_client, sizeof(uint64_t));
      memcpy(&end, from_client + sizeof(uint64_t), sizeof(uint64_t));
      memcpy(&mod, from_client + 2 * sizeof(uint64_t), sizeof(uint64_t));

      fprintf(stdout, "Receive: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
              begin, end, mod);

      if (mod == 0) {
        uint64_t zero = 0;
        send_all(client_fd, &zero, sizeof(zero));
        continue;
      }

      pthread_t threads[tnum];
      struct FactorialArgs args[tnum];

      uint64_t total_elems = (end >= begin) ? (end - begin + 1) : 0;
      uint64_t chunk = total_elems / (uint64_t)tnum;
      uint64_t rem = total_elems % (uint64_t)tnum;
      uint64_t cur = begin;

      for (uint32_t i = 0; i < (uint32_t)tnum; i++) {
        uint64_t len = chunk + ((uint64_t)i < rem ? 1 : 0);

        if (len == 0) {
          args[i].begin = 1;
          args[i].end = 0;
        } else {
          args[i].begin = cur;
          args[i].end = cur + len - 1;
          cur += len;
        }

        args[i].mod = mod;

        if (pthread_create(&threads[i], NULL, ThreadFactorial,
                           (void *)&args[i])) {
          printf("Error: pthread_create failed!\n");
          return 1;
        }
      }

      uint64_t total = 1 % mod;

      for (uint32_t i = 0; i < (uint32_t)tnum; i++) {
        void *thread_result = NULL;
        pthread_join(threads[i], &thread_result);

        uint64_t result = (uint64_t)(uintptr_t)thread_result;
        total = MultModulo(total, result, mod);
      }

      printf("Total: %" PRIu64 "\n", total);

      if (send_all(client_fd, &total, sizeof(total)) < 0) {
        fprintf(stderr, "Can't send data to client\n");
        break;
      }
    }

    shutdown(client_fd, SHUT_RDWR);
    close(client_fd);
  }

  return 0;
}