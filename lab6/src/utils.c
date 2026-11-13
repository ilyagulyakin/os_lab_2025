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

  return (uint64_t)((unsigned __int128)(a % mod) * (b % mod) % mod);
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

struct FactorialArgs SplitRange(uint64_t begin, uint64_t end, uint64_t parts,
                                size_t index) {
  struct FactorialArgs part = {1, 0, 0};

  if (parts == 0 || end < begin) {
    return part;
  }

  uint64_t total = end - begin + 1;
  uint64_t chunk = total / parts;
  uint64_t rem = total % parts;
  uint64_t idx = (uint64_t)index;

  uint64_t extra_before = idx < rem ? idx : rem;
  uint64_t len = chunk + (idx < rem ? 1 : 0);

  if (len != 0) {
    part.begin = begin + idx * chunk + extra_before;
    part.end = part.begin + len - 1;
  }

  return part;
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