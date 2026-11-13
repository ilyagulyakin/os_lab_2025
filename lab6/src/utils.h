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

_Static_assert(sizeof(struct FactorialArgs) == 3 * sizeof(uint64_t),
               "wire format must stay 24 bytes");

uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);
uint64_t Factorial(const struct FactorialArgs *args);
bool ConvertStringToUI64(const char *str, uint64_t *val);

/* i-я из parts частей диапазона [begin; end].
 * Пустая часть (в том числе при begin > end) — {1, 0}:
 * нейтральный элемент умножения, результат не меняется. */
struct FactorialArgs SplitRange(uint64_t begin, uint64_t end, uint64_t parts,
                                size_t index);

/* send_all: 0 - успех, -1 - ошибка.
 * recv_all: 1 - успех, 0 - соединение закрыто, -1 - ошибка. */
int send_all(int fd, const void *buf, size_t len);
int recv_all(int fd, void *buf, size_t len);

#endif