#pragma once
#include "stdint.h"

typedef uint8_t qfloat_u8;
typedef double qfloat_f64;
typedef intptr_t qfloat_intptr;
typedef uintptr_t qfloat_uintptr;
_Static_assert(sizeof(qfloat_f64) == 8, "qfloat_f64");

#ifdef assert
  #define qfloat_assert(condition) assert(condition)
#else
  #define qfloat_assert(condition)
#endif

/* NOTE: f64 needs at most 17 (integer+fraction) digits: https://www.exploringbinary.com/number-of-digits-required-for-round-trip-conversions/
    EXPLICIT_MANTISSA_BITS_f64 = 52
    Math.ceil(EXPLICIT_MANTISSA_BITS_f64 * Math.log10(2)) + 1 = 17 */
/* NOTE: sign(1) + digits(17) + decimal_point(1) + signed_exponent(5) + null_terminator(1) */
#define QFLOAT_SIZE_f64 ((qfloat_uintptr)(1) + 17 + 1 + 5 + 1)
typedef qfloat_f64 qfloat_str_to_f64(char *buffer, qfloat_uintptr size);

#if QFLOAT_HAS_CRT
  #include <math.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
void shorten_f64_string(qfloat_f64 value, char buffer[QFLOAT_SIZE_f64], qfloat_uintptr size, qfloat_str_to_f64 str_to_float) {
  qfloat_assert(size < QFLOAT_SIZE_f64);
  // preserve "+-inf", "nan"
  if (size == 0) return;
  qfloat_intptr start = buffer[0] == '-' || buffer[0] == '+' ? 1 : 0;
  if (buffer[start] < '0' || buffer[start] > '9') return;
  // find exponent
  qfloat_intptr exponent_index = 0;
  while (exponent_index < size && buffer[exponent_index] != 'e') {
    exponent_index++;
  }
  // round up and trucate
  char shortened[QFLOAT_SIZE_f64];
  memcpy(shortened, buffer, size);
  while (exponent_index > 1) {
    /* NOTE: here we produce a wrong result if it would overflow to a new digit,
       but then we discard it anyway, so we don't actually care */
    qfloat_u8 carry = 1;
    for (intptr_t j = exponent_index - 2; j >= 0; j--) {
      char c = shortened[j];
      if (c < '0' || c > '9') continue;
      qfloat_u8 digit = (c - '0') + carry;
      shortened[j] = '0' + (digit % 10);
      carry = digit >= 10 ? 1 : 0;
    }
    memcpy(shortened + exponent_index - 1, buffer + exponent_index, size - (qfloat_uintptr)exponent_index);
    shortened[size - 1] = '\0';
    // if valid transform, copy to original
    if (str_to_float(shortened, size - 1) != value) break;
    memcpy(buffer, shortened, size);
    exponent_index--;
    size--;
  }
  // truncate
  qfloat_intptr significand_end = exponent_index;
  qfloat_intptr exponent_end = (qfloat_intptr)size;
  memcpy(shortened, buffer, (qfloat_uintptr)exponent_index);
  while (significand_end > 1) {
    memcpy(shortened + significand_end - 1, buffer + exponent_index, size - (qfloat_uintptr)exponent_index);
    shortened[exponent_end - 1] = '\0';
    // if valid transform, update significand_end
    if (str_to_float(shortened, (qfloat_uintptr)exponent_end - 1) != value) break;
    significand_end--;
    exponent_end--;
  }
  memcpy(buffer + significand_end, buffer + exponent_index, size - (qfloat_uintptr)exponent_index);
  buffer[exponent_end] = '\0';
}
qfloat_f64 str_to_f64_libc(char *buffer, qfloat_uintptr size) {
  return strtod(buffer, 0);
}
void sprint_f64_libc(qfloat_f64 value, char buffer[QFLOAT_SIZE_f64]) {
  int size = snprintf(buffer, QFLOAT_SIZE_f64, "%.17g", value);
  shorten_f64_string(value, buffer, (qfloat_uintptr)size, str_to_f64_libc);
}
#endif

// IEEE "augmented arithmetic operations"
/* NOTE: pragma to disable float optimizations for these functions */
#pragma STDC FENV_ACCESS ON
inline __attribute__((always_inline)) qfloat_f64 qfloat_fma(qfloat_f64 a, qfloat_f64 b, qfloat_f64 c) {
#if !(__x86_64__ || __i386__)
  _Static_assert(false, "Not implemented");
#endif
  qfloat_f64 result = a;
  asm volatile("vfmadd213sd %0, %1, %2" : "+x"(result) : "x"(b), "x"(c));
  return result;
}
void augmented_mul(qfloat_f64 a, qfloat_f64 b, qfloat_f64 *result, qfloat_f64 *error) {
  qfloat_f64 p = a * b;
  *error = qfloat_fma(a, b, -p); /* NOTE: fma() has infinite precision for `a*b` */
  *result = p;
}
void augmented_fma(qfloat_f64 x, qfloat_f64 y, qfloat_f64 z, qfloat_f64 *result, qfloat_f64 *error) {
  qfloat_f64 p = qfloat_fma(x, y, z);
  *error = qfloat_fma(x, y, z - p);
  *result = p;
}
void augmented_add(qfloat_f64 a, qfloat_f64 b, qfloat_f64 *result, qfloat_f64 *error) {
#if 0
  augmented_fma(1.0, a, b, result, error);
#else
  /* NOTE: float math must be strict here! */
  qfloat_f64 s = a + b;
  qfloat_f64 bb = s - a;
  *error = (a - (s - bb)) + (b - bb);
  *result = s;
#endif
}
void augmented_add_fast(qfloat_f64 a, qfloat_f64 b, qfloat_f64 *result, qfloat_f64 *error) {
  qfloat_assert(fabs(a) >= fabs(b));
  /* NOTE: float math must be strict here! */
  qfloat_f64 s = a + b;
  *error = b - (s - a);
  *result = s;
}

// TODO: format and parse floats without libc
#pragma STDC FENV_ACCESS DEFAULT
