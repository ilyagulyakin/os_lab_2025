#ifndef UTILS_H
#define UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct FactorialArgs {
  uint64_t begin;
  uint64_t end;
  uint64_t mod;
};

/* (a * b) % mod без переполнения при умножении */
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

/* (begin * (begin+1) * ... * end) % mod */
uint64_t Factorial(const struct FactorialArgs *args);

/* Разбор строки в uint64_t; false при ошибке */
bool ConvertStringToUI64(const char *str, uint64_t *val);

/* Отправить/принять ровно len байт.
 * send_all: 0 - успех, -1 - ошибка.
 * recv_all: 1 - успех, 0 - соединение закрыто, -1 - ошибка. */
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);

#endif