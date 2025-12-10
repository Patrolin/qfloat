// https://github.com/Patrolin/qfloat
#pragma once
#include "stdint.h"
#include "stdbool.h"

typedef uint8_t qfloat_u8;
typedef uint64_t qfloat_u64;
typedef double qfloat_f64;
typedef intptr_t qfloat_intptr;
typedef uintptr_t qfloat_uintptr;
_Static_assert(sizeof(qfloat_f64) == 8, "qfloat_f64");

#ifndef qfloat_assert
  #ifdef assert
    #define qfloat_assert(condition) assert(condition)
  #else
    #define qfloat_assert(condition)
  #endif
#endif

// overflow: https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
#define qfloat_add_overflow(a, b, dest) __builtin_add_overflow(a, b, dest)
#define qfloat_mul_overflow(a, b, dest) __builtin_mul_overflow(a, b, dest)

#define EXPLICIT_MANTISSA_BITS_f64 52
#define IMPLICIT_MANTISSA_BITS_f64 53
/* NOTE: f64 needs at most 17 (integer+fraction) digits: https://www.exploringbinary.com/number-of-digits-required-for-round-trip-conversions/
    Math.ceil(EXPLICIT_MANTISSA_BITS_f64 * Math.log10(2)) + 1 = 17 */
/* NOTE: sign(1) + digits(17) + decimal_point(1) + signed_exponent(5) + null_terminator(1) */
#define QFLOAT_SIZE_f64 ((qfloat_intptr)(1) + 17 + 1 + 5 + 1)
typedef qfloat_f64 qfloat_str_to_f64(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end);

// libc
#if !QFLOAT_NOLIBC
  #include <math.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #define qfloat_copy(ptr, size, dest) memcpy(dest, ptr, size)
#endif
qfloat_intptr qfloat_shorten_f64_string(qfloat_f64 value, char buffer[_Nonnull static QFLOAT_SIZE_f64], qfloat_intptr size, qfloat_str_to_f64 str_to_float) {
  qfloat_assert(size < QFLOAT_SIZE_f64);
  // preserve "+-inf", "nan"
  if (size == 0) return size;
  qfloat_intptr start = buffer[0] == '-' || buffer[0] == '+' ? 1 : 0;
  if (buffer[start] < '0' || buffer[start] > '9') return size;
  // find exponent
  qfloat_intptr exponent_index = 0;
  while (exponent_index < size && buffer[exponent_index] != 'e') {
    exponent_index++;
  }
  // truncate significand upwards
  char shortened[QFLOAT_SIZE_f64];
  qfloat_copy(buffer, (qfloat_uintptr)size, shortened);
  qfloat_intptr _end;
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
    qfloat_copy(buffer + exponent_index, (qfloat_uintptr)(size - exponent_index), shortened + exponent_index - 1);
    shortened[size - 1] = '\0';
    // if valid transform, copy to original
    if (str_to_float(shortened, size - 1, 0, &_end) != value) break;
    qfloat_copy(shortened, (qfloat_uintptr)size, buffer);
    exponent_index--;
    size--;
  }
  // truncate significand
  qfloat_intptr significand_end = exponent_index;
  qfloat_intptr exponent_end = (qfloat_intptr)size;
  qfloat_copy(buffer, (qfloat_uintptr)exponent_index, shortened);
  while (significand_end > 1) {
    qfloat_copy(buffer + exponent_index, (qfloat_uintptr)(size - exponent_index), shortened + significand_end - 1);
    shortened[exponent_end - 1] = '\0';
    // if valid transform, update significand_end
    if (str_to_float(shortened, exponent_end - 1, 0, &_end) != value) break;
    significand_end--;
    exponent_end--;
  }
  qfloat_copy(buffer + exponent_index, (qfloat_uintptr)(size - exponent_index), buffer + significand_end);
  buffer[exponent_end] = '\0';
  return exponent_end;
}
#if !QFLOAT_NOLIBC
qfloat_f64 qfloat_str_to_f64_libc(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  return strtod(str, 0);
}
qfloat_intptr qfloat_sprint_f64_libc(qfloat_f64 value, char buffer[_Nonnull static QFLOAT_SIZE_f64]) {
  int size = snprintf(buffer, QFLOAT_SIZE_f64, "%.17g", value);
  return qfloat_shorten_f64_string(value, buffer, size, qfloat_str_to_f64_libc);
}
// TODO: f32 support?
#endif

// IEEE "augmented arithmetic operations"
/* NOTE: pragma to disable float optimizations for these functions */
#pragma STDC FENV_ACCESS ON
inline __attribute__((always_inline)) qfloat_f64 qfloat_fma_f64(qfloat_f64 a, qfloat_f64 b, qfloat_f64 c) {
#if !(__x86_64__ || __i386__)
  _Static_assert(false, "Not implemented");
#endif
  qfloat_f64 result = a;
  asm volatile("vfmadd213sd %0, %1, %2" : "+x"(result) : "x"(b), "x"(c));
  return result;
}
qfloat_f64 augmented_mul_f64(qfloat_f64 a, qfloat_f64 b, qfloat_f64 *_Nonnull error) {
  qfloat_f64 result = a * b;
  *error = qfloat_fma_f64(a, b, -result); /* NOTE: fma() has infinite precision for `a*b` */
  return result;
}
qfloat_f64 augmented_fma_f64(qfloat_f64 x, qfloat_f64 y, qfloat_f64 z, qfloat_f64 *_Nonnull error) {
  qfloat_f64 result = qfloat_fma_f64(x, y, z);
  *error = qfloat_fma_f64(x, y, z - result);
  return result;
}
qfloat_f64 augmented_add_f64(qfloat_f64 a, qfloat_f64 b, qfloat_f64 *_Nonnull error) {
#if 0
  augmented_fma(1.0, a, b, result, error);
#else
  qfloat_f64 result = a + b;
  /* NOTE: float optimizations are disabled here */
  qfloat_f64 bb = result - a;
  *error = (a - (result - bb)) + (b - bb);
  return result;
#endif
}
qfloat_f64 augmented_add_fast_f64(qfloat_f64 a, qfloat_f64 b, qfloat_f64 *_Nonnull error) {
  qfloat_assert(fabs(a) >= fabs(b));
  qfloat_f64 result = a + b;
  /* NOTE: float optimizations are disabled here */
  *error = b - (result - a);
  return result;
}

// nolibc
qfloat_u64 qfloat_parse_u64_hex(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  qfloat_u64 result = 0;
  qfloat_intptr i = start;
  while (i < str_size) {
    char c = str[i];
    qfloat_u8 digit = 16;
    switch (c) {
      case '0' ... '9':
        digit = c - '0';
        break;
      case 'a' ... 'f':
        digit = c - 'a' + 10;
        break;
      case 'A' ... 'F':
        digit = c - 'A' + 10;
        break;
    }
    qfloat_u64 new_result;
    bool did_overflow = qfloat_mul_overflow(result, 16, &new_result);
    did_overflow |= qfloat_add_overflow(new_result, (qfloat_u64)digit, &new_result);
    if (digit >= 16 || did_overflow) break;
    result = new_result;
    i++;
  }
  *end = i;
  return result;
}
qfloat_u64 qfloat_parse_u64_decimal(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  qfloat_u64 result = 0;
  qfloat_intptr i = start;
  while (i < str_size) {
    char digit = str[i] - '0';
    qfloat_u64 new_result;
    bool did_overflow = qfloat_mul_overflow(result, 10, &new_result);
    did_overflow |= qfloat_add_overflow(new_result, (qfloat_u64)digit, &new_result);
    if (digit >= 10 || new_result < result) break;
    result = new_result;
    i++;
  }
  *end = i;
  return result;
}
qfloat_f64 qfloat_parse_fraction64_decimal(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  // find end
  qfloat_intptr i = start;
  while (i < str_size) {
    char digit = str[i] - '0';
    if (digit > 10) break;
    i++;
  }
  *end = i;
  // compute result from smallest to largest
  qfloat_f64 result = 0.0;
  while (i > start) {
    i--;
    char digit = str[i] - '0';
    result = result * 0.1 + (qfloat_f64)digit;
  }
  return result * 0.1;
}
qfloat_f64 qfloat_parse_f64_decimal(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  qfloat_intptr i = start;
  // sign
  bool negative = i < str_size && str[i] == '-';
  if (negative) i++;
  // mantissa
  qfloat_f64 mantissa_f64 = (qfloat_f64)qfloat_parse_u64_decimal(str, str_size, i, &i);
  if (i < str_size && str[i] == '.') {
    mantissa_f64 += qfloat_parse_fraction64_decimal(str, str_size, i + 1, &i);
  }
  // exponent
  qfloat_f64 exponent_f64 = 1.0;
  bool exponent_is_negative = false;
  if (i < str_size && str[i] == 'e') {
    i++;
    if (i < str_size && str[i] == '-') {
      exponent_is_negative = true;
      i++;
    }
    // TODO: use augmented ops for better precision
    qfloat_u64 base10_exponent = (qfloat_u64)qfloat_parse_u64_decimal(str, str_size, i, &i);
    qfloat_f64 next_pow10_step = 10.0;
    while (base10_exponent > 0) {
      if ((base10_exponent & 1) == 1) {
        exponent_f64 = exponent_f64 * next_pow10_step;
      }
      next_pow10_step = next_pow10_step * next_pow10_step;
      base10_exponent = base10_exponent >> 1;
    }
  }
  // compose float
  qfloat_f64 value = mantissa_f64 * exponent_f64;
  if (exponent_is_negative) {
    value = mantissa_f64 / exponent_f64;
  }
  value = negative ? -value : value;
  *end = i;
  return value;
}
qfloat_f64 str_to_f64(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  if (start + 1 < str_size && str[start] == '0' && str[start] == 'x') {
    qfloat_u64 hex_value = qfloat_parse_u64_hex(str, str_size, start, end);
    qfloat_f64 value;
    memcpy(&value, &hex_value, sizeof(value));
    return value;
  } else {
    return qfloat_parse_f64_decimal(str, str_size, start, end);
  }
}
qfloat_intptr sprint_f64(qfloat_f64 value, char buffer[_Nonnull static QFLOAT_SIZE_f64]) {
  /* TODO: format without libc */
  int size = snprintf(buffer, QFLOAT_SIZE_f64, "%.17g", value);
  return qfloat_shorten_f64_string(value, buffer, size, qfloat_parse_f64_decimal);
}
/* NOTE: overwrite FENV_ACCESS pragma to default value */
#pragma STDC FENV_ACCESS DEFAULT
