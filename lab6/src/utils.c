#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>

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

bool ConvertStringToUI64(const char *str, uint64_t *val) {
  char *end = NULL;
  errno = 0;

  unsigned long long i = strtoull(str, &end, 10);

  if (errno == ERANGE) {
    fprintf(stderr, "Out of uint64_t range: %s\n", str);
    return false;
  }

  if (errno != 0 || end == str || *end != '\0') {
    return false;
  }

  *val = (uint64_t)i;
  return true;
}

int send_all(int fd, const void *buf, size_t len) {
  const char *p = (const char *)buf;
  size_t off = 0;

  while (off < len) {
    ssize_t n = send(fd, p + off, len - off, MSG_NOSIGNAL);
    if (n <= 0) {
      return -1;
    }
    off += (size_t)n;
  }

  return 0;
}

int recv_all(int fd, void *buf, size_t len) {
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