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
          "Using: %s --ip ADDRESS --port PORT [--bufsize BYTES]\n"
          "  --ip, -a       IPv4 address of the server\n"
          "  --port, -p     TCP port of the server (1..65535)\n"
          "  --bufsize, -b  read buffer size in bytes, 1..%d (default %d)\n",
          prog, MAX_BUFSIZE, DEFAULT_BUFSIZE);
}

static int SendAll(int fd, const char *buf, size_t len) {
  size_t off = 0;

  while (off < len) {
    ssize_t n = send(fd, buf + off, len - off, MSG_NOSIGNAL);
    if (n < 0) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    off += (size_t)n;
  }

  return 0;
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

  int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    perror("socket");
    return 1;
  }

  if (connect(fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
    perror("connect");
    close(fd);
    return 1;
  }

  char *buf = malloc((size_t)bufsize);
  if (buf == NULL) {
    perror("malloc");
    close(fd);
    return 1;
  }

  if (write(STDOUT_FILENO, "Input message to send\n", 22) < 0) {
    perror("write");
    free(buf);
    close(fd);
    return 1;
  }

  ssize_t nread;
  while ((nread = read(STDIN_FILENO, buf, (size_t)bufsize)) > 0) {
    if (SendAll(fd, buf, (size_t)nread) < 0) {
      perror("write");
      free(buf);
      close(fd);
      return 1;
    }
  }

  if (nread < 0) {
    perror("read");
    free(buf);
    close(fd);
    return 1;
  }

  free(buf);
  close(fd);
  return 0;
}
