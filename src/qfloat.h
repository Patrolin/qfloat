// https://github.com/Patrolin/qfloat
#pragma once
#include "stdint.h"
#include "stdbool.h"

typedef uint8_t qfloat_u8;
typedef uint64_t qfloat_u64;
typedef int64_t qfloat_i64;
typedef double qfloat_f64;
typedef intptr_t qfloat_intptr;
typedef uintptr_t qfloat_uintptr;
_Static_assert(sizeof(qfloat_f64) == 8, "qfloat_f64");

#ifndef qfloat_assert
  #define qfloat_assert(condition) assert(condition)
#endif

// overflow: https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
#define qfloat_add_overflow(a, b, dest) __builtin_add_overflow(a, b, dest)
#define qfloat_mul_overflow(a, b, dest) __builtin_mul_overflow(a, b, dest)

#define QFLOAT_EXPLICIT_MANTISSA_BITS_f64 52
#define QFLOAT_IMPLICIT_MANTISSA_BITS_f64 53
#define QFLOAT_EXPONENT_BITS_f64 (64 - IMPLICIT_MANTISSA_BITS_f64)
/* NOTE: f64 needs at most 17 (integer+fraction) digits: https://www.exploringbinary.com/number-of-digits-required-for-round-trip-conversions/
    Math.ceil(EXPLICIT_MANTISSA_BITS_f64 * Math.log10(2)) + 1 = 17 */
#define QFLOAT_BASE10_DIGITS_f64 17
/* NOTE: sign(1) + digits(17) + decimal_point(1) + signed_exponent(5) + null_terminator(1) */
#define QFLOAT_SIZE_f64 ((qfloat_intptr)(1) + QFLOAT_BASE10_DIGITS_f64 + 1 + 5 + 1)
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
/* NOTE: fma() has infinite precision for `a*b` */
inline __attribute__((always_inline)) qfloat_f64 qfloat_fma_f64(qfloat_f64 a, qfloat_f64 b, qfloat_f64 c) {
#if !(__x86_64__ || __i386__)
  _Static_assert(false, "Not implemented");
#endif
  qfloat_f64 result = a;
  asm volatile("vfmadd213sd %0, %1, %2" : "+x"(result) : "x"(b), "x"(c));
  return result;
}
/* NOTE: these fail (give a slightly incorrect result) for `abs(x) < 1e-303`, but who gives af */
typedef struct {
  qfloat_f64 high;
  qfloat_f64 low;
} qfloat_dd;
qfloat_dd augmented_mul_f64(qfloat_dd a, qfloat_f64 b) {
  // multiply
  qfloat_f64 result = a.high * b;
  qfloat_f64 error = qfloat_fma_f64(a.high, b, -result) + (a.low * b);
  // output
  qfloat_f64 new_high = result + error;
  qfloat_f64 new_low = error - (new_high - result);
  return (qfloat_dd){new_high, new_low};
}
qfloat_dd augmented_div_f64(qfloat_dd a, qfloat_f64 b) {
  // divide
  qfloat_f64 result = a.high / b;
  qfloat_f64 error = (qfloat_fma_f64(-result, b, a.high) + a.low) / b;
  // output
  qfloat_f64 new_high = result + error;
  qfloat_f64 new_low = error - (new_high - result);
  return (qfloat_dd){new_high, new_low};
}
/*qfloat_dd augmented_add_fast_f64(qfloat_dd a, qfloat_f64 b) {
  qfloat_assert(fabs(a.high) >= fabs(b));
  // add
  qfloat_f64 result = a.high + b;
  qfloat_f64 error = b - (result - a.high) + (a.low);
  // output
  qfloat_f64 new_high = result + error;
  qfloat_f64 new_low = error - (new_high - result);
  return (qfloat_dd){new_high, new_low};
}*/

// nolibc
qfloat_u64 qfloat_parse_u64_decimal(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  qfloat_u64 result = 0;
  qfloat_intptr i = start;
  while (i < str_size) {
    char digit = str[i] - '0';
    qfloat_u64 new_result;
    bool did_overflow = qfloat_mul_overflow(result, 10, &new_result);
    did_overflow |= qfloat_add_overflow(new_result, (qfloat_u64)digit, &new_result);
    if (digit >= 10 || did_overflow) break;
    result = new_result;
    i++;
  }
  *end = i;
  return result;
}
qfloat_i64 qfloat_parse_i64_decimal(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  /* NOTE: `-MIN(i64)` cannot be represented in a `u64` */
  qfloat_i64 result = 0;
  qfloat_intptr i = start;
  // sign
  bool negative;
  if (str[i] == '-' || str[i] == '+') {
    negative = str[i] == '-';
    i++;
  }
  // value
  while (i < str_size) {
    char digit = str[i] - '0';
    qfloat_i64 new_result;
    bool did_overflow = qfloat_mul_overflow(result, 10, &new_result);
    did_overflow |= qfloat_add_overflow(new_result, (qfloat_i64)digit, &new_result);
    if (digit >= 10 || did_overflow) break;
    result = new_result;
    i++;
  }
  *end = i;
  return negative ? -result : result;
}
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
qfloat_dd qfloat_parse_f64_significand(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end, qfloat_intptr *_Nonnull exponent_offset_ptr) {
  qfloat_i64 result = 0;
  qfloat_intptr i = start;
  // integer
  qfloat_intptr nonzero_digits = 0;
  while (i < str_size && str[i] == '0') {
    i++;
  }
  while (i < str_size && nonzero_digits < QFLOAT_BASE10_DIGITS_f64) {
    char digit = str[i] - '0';
    qfloat_i64 new_result = result * 10 + digit;
    if (digit >= 10) break;
    result = new_result;
    nonzero_digits++;
    i++;
  }
  // fraction
  qfloat_intptr exponent_offset = 0;
  if (i < str_size && str[i] == '.') {
    i++;
    if (result == 0) {
      while (i < str_size && str[i] == '0') {
        exponent_offset--;
        i++;
      }
    }
    while (i < str_size && nonzero_digits < QFLOAT_BASE10_DIGITS_f64) {
      char digit = str[i] - '0';
      qfloat_i64 new_result = result * 10 + digit;
      if (digit >= 10) break;
      result = new_result;
      nonzero_digits++;
      exponent_offset--;
      i++;
    }
  }
  *end = i;
  *exponent_offset_ptr = exponent_offset;
  qfloat_dd result_dd;
  result_dd.high = (qfloat_f64)result;
  qfloat_i64 result_rounded = (qfloat_i64)result_dd.high;
  result_dd.low = (qfloat_f64)(result - result_rounded);
  return result_dd;
}
// TODO: use a table?
qfloat_f64 SAFE_POWERS_OF_10[22] = {
    1e1,
    1e2,
    1e3,
    1e4,
    1e5,
    1e6,
    1e7,
    1e8,
    1e9,
    1e10,
    1e11,
    1e12,
    1e13,
    1e14,
    1e15,
    1e16,
    1e17,
    1e18,
    1e19,
    1e20,
    1e21,
    1e22,
};
qfloat_f64 qfloat_parse_f64_decimal(const char *_Nonnull str, qfloat_intptr str_size, qfloat_intptr start, qfloat_intptr *_Nonnull end) {
  // sign
  qfloat_intptr i = start;
  bool negative = i < str_size && str[i] == '-';
  if (negative) i++;
  /* TODO: parse inf, nan */
  // significand
  qfloat_intptr exponent_base10;
  qfloat_dd value = qfloat_parse_f64_significand(str, str_size, i, &i, &exponent_base10);
#ifndef NDEBUG
  printf("\nstr:        %s", str);
  printf("\nsignificand: %.17g, %.17g, %lli", value.high, value.low, exponent_base10);
#endif
  // exponent_base10
  if (i < str_size && str[i] == 'e') {
    i++;
    exponent_base10 += qfloat_parse_i64_decimal(str, str_size, i, &i);
  }
#ifndef NDEBUG
  printf("\nexponent: %lli", exponent_base10);
#endif
  while (exponent_base10 < 0) {
    value = augmented_div_f64(value, 10.0);
    exponent_base10++;
  }
  while (exponent_base10 > 0) {
    value = augmented_mul_f64(value, 10.0);
    exponent_base10--;
  }
  *end = i;
  qfloat_f64 x = value.high + value.low;  // TODO: remove this probably
  return negative ? -x : x;
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
