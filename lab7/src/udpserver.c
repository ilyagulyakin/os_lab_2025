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

#define DEFAULT_BUFSIZE 1024
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
          "  --port, -p     UDP port to listen on (1..65535)\n"
          "  --bufsize, -b  receive buffer size in bytes, 1..%d (default %d)\n",
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

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons((uint16_t)port);

  if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("bind");
    close(sockfd);
    return 1;
  }

  /* +1 байт под завершающий ноль: иначе mesg[bufsize] выходит за границу */
  char *mesg = malloc((size_t)bufsize + 1);
  if (mesg == NULL) {
    perror("malloc");
    close(sockfd);
    return 1;
  }

  printf("SERVER starts on port %d (bufsize %d)...\n", port, bufsize);
  fflush(stdout);

  for (;;) {
    struct sockaddr_in cliaddr;
    socklen_t len = sizeof(cliaddr);

    ssize_t n = recvfrom(sockfd, mesg, (size_t)bufsize, 0,
                         (struct sockaddr *)&cliaddr, &len);
    if (n < 0) {
      perror("recvfrom");
      break;
    }
    mesg[n] = '\0';

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cliaddr.sin_addr, ip, sizeof(ip));

    printf("REQUEST %s      FROM %s : %d\n", mesg, ip, ntohs(cliaddr.sin_port));
    fflush(stdout);

    if (sendto(sockfd, mesg, (size_t)n, 0, (struct sockaddr *)&cliaddr, len) <
        0) {
      perror("sendto");
      break;
    }
  }

  free(mesg);
  close(sockfd);
  return 0;
}
