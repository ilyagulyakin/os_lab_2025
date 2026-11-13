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
          "Using: %s --ip ADDRESS --port PORT [--bufsize BYTES]\n"
          "  --ip, -a       IPv4 address of the server\n"
          "  --port, -p     UDP port of the server (1..65535)\n"
          "  --bufsize, -b  message buffer size in bytes, 1..%d (default %d)\n",
          prog, MAX_BUFSIZE, DEFAULT_BUFSIZE);
}

int main(int argc, char **argv) {
  const char *ip = NULL;
  int port = 0;
  int bufsize = DEFAULT_BUFSIZE;

  static struct option options[] = {
      {"ip", required_argument, 0, 'a'},
      {"port", required_argument, 0, 'p'},
      {"bufsize", required_argument, 0, 'b'},
      {"help", no_argument, 0, 'h'},
      {0, 0, 0, 0},
  };

  int opt;
  while ((opt = getopt_long(argc, argv, "a:p:b:h", options, NULL)) != -1) {
    switch (opt) {
    case 'a':
      ip = optarg;
      break;

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

  if (ip == NULL || port == 0) {
    Usage(argv[0]);
    return 1;
  }

  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons((uint16_t)port);

  if (inet_pton(AF_INET, ip, &servaddr.sin_addr) <= 0) {
    fprintf(stderr, "bad address: %s\n", ip);
    return 1;
  }

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return 1;
  }

  char *sendline = malloc((size_t)bufsize);
  char *recvline = malloc((size_t)bufsize + 1);
  if (sendline == NULL || recvline == NULL) {
    perror("malloc");
    free(sendline);
    free(recvline);
    close(sockfd);
    return 1;
  }

  if (write(STDOUT_FILENO, "Enter string\n", 13) < 0) {
    perror("write");
    free(sendline);
    free(recvline);
    close(sockfd);
    return 1;
  }

  ssize_t n;
  while ((n = read(STDIN_FILENO, sendline, (size_t)bufsize)) > 0) {
    if (sendto(sockfd, sendline, (size_t)n, 0, (struct sockaddr *)&servaddr,
               sizeof(servaddr)) < 0) {
      perror("sendto");
      free(sendline);
      free(recvline);
      close(sockfd);
      return 1;
    }

    ssize_t reply = recvfrom(sockfd, recvline, (size_t)bufsize, 0, NULL, NULL);
    if (reply < 0) {
      perror("recvfrom");
      free(sendline);
      free(recvline);
      close(sockfd);
      return 1;
    }
    recvline[reply] = '\0';

    printf("REPLY FROM SERVER= %s\n", recvline);
  }

  free(sendline);
  free(recvline);
  close(sockfd);
  return 0;
}
