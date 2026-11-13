#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <getopt.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_BUFSIZE 100
#define MAX_BUFSIZE (1024 * 1024)

static int ParseInt(const char *str, int min, int max, int *out) {
  char *end = NULL;
  errno = 0;

  long value = strtol(str, &end, 10);
  if (errno != 0 || end == str || *end != '\0' || value < min || value > max) {
    return 0;
  }

  *out = (int)value;
  return 1;
}

static void Usage(const char *prog) {
  fprintf(stderr,
          "Using: %s --port PORT [--bufsize BYTES]\n"
          "  --port, -p     TCP port to listen on (1..65535)\n"
          "  --bufsize, -b  read buffer size in bytes, 1..%d (default %d)\n",
          prog, MAX_BUFSIZE, DEFAULT_BUFSIZE);
}

int main(int argc, char **argv) {
  int port = 0;
  int bufsize = DEFAULT_BUFSIZE;

  static struct option options[] = {
      {"port", required_argument, 0, 'p'},
      {"bufsize", required_argument, 0, 'b'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "p:b:h", options, NULL)) != -1) {
    switch (opt) {
    case 'p':
      if (!ParseInt(optarg, 1, 65535, &port)) {
        fprintf(stderr, "Invalid port: %s\n", optarg);
        return 1;
      }
      break;

    case 'b':
      if (!ParseInt(optarg, 1, MAX_BUFSIZE, &bufsize)) {
        fprintf(stderr, "Invalid buffer size: %s\n", optarg);
        return 1;
      }
      break;

    case 'h':
      Usage(argv[0]);
      return 0;

    default:
      Usage(argv[0]);
      return 1;
    }
  }

  if (port == 0) {
    Usage(argv[0]);
    return 1;
  }

  int lfd = socket(AF_INET, SOCK_STREAM, 0);
  if (lfd < 0) {
    perror("socket");
    return 1;
  }

  int one = 1;
  setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons((uint16_t)port);

  if (bind(lfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind");
    close(lfd);
    return 1;
  }

  if (listen(lfd, 5) < 0) {
    perror("listen");
    close(lfd);
    return 1;
  }

  char *buf = malloc((size_t)bufsize);
  if (buf == NULL) {
    perror("malloc");
    close(lfd);
    return 1;
  }

  printf("SERVER starts on port %d (bufsize %d)...\n", port, bufsize);
  fflush(stdout);

  for (;;) {
    struct sockaddr_in cliaddr;
    socklen_t clilen = sizeof(cliaddr);

    int cfd = accept(lfd, (struct sockaddr *)&cliaddr, &clilen);
    if (cfd < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("accept");
      break;
    }

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));
    printf("connection established: %s:%d\n", ip, ntohs(cliaddr.sin_port));
    fflush(stdout);

    ssize_t nread;
    while ((nread = read(cfd, buf, (size_t)bufsize)) > 0) {
      if (write(STDOUT_FILENO, buf, (size_t)nread) != nread) {
        perror("write");
        break;
      }
    }

    if (nread < 0) {
      perror("read");
    }

    close(cfd);
  }

  free(buf);
  close(lfd);
  return 0;
}
