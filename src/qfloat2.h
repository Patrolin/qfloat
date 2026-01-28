// https://github.com/Patrolin/qfloat
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h> // TODO: remove logging

// common
#if defined(_MSC_VER) && !defined(__clang__)
/* NOTE: MSVC is kind of a lost cause, so we're not supporting it,
  technically you could implement the `__builtin_xx_yy()`s yourself with `_Generic()` */
#endif
#ifdef NOLIBC
void *memcpy(void *dest, const void *src, size_t n);
#else
  #include <assert.h>
  #include <string.h> /* NOTE: for memcpy() */
#endif
#ifndef qf_assert
  #define qf_assert(condition) assert(condition)
#endif
#ifndef QF_ASSERT
  #define QF_ASSERT(condition) _Static_assert(condition, #condition)
  #define qf_nonnull(...)      __attribute__((nonnull(__VA_ARGS__)))
  #define qf_near(condition)   __builtin_expect(condition, true)
  #define qf_far(condition)    __builtin_expect(condition, false)
  #define QF_CONCAT_RAW(a, b)  a##b
  #define QF_CONCAT(a, b)      QF_CONCAT_RAW(a, b)
  #define qf_bitcopy(src, dest)                   \
    QF_ASSERT(sizeof(*(src)) == sizeof(*(dest))); \
    memcpy(dest, src, sizeof(*(src)));
  #define qf_abs(a)                 ((a) < 0 ? -(a) : (a))
  #define qf_min(a, b)              ((a) < (b) ? (a) : (b))
  #define qf_count_leading_zeros(a) ((__typeof__(a))__builtin_clzg(a))
QF_ASSERT(sizeof(char) == 1);
#endif

// types
typedef double qf_f64;
QF_ASSERT(sizeof(qf_f64) == 8);
/* NOTE: QF_MAX_SIGNIFICAND_DIGITS_xx = Math.ceil(EXPLICIT_MANTISSA_BITS_xx * Math.log10(2)) */
#define QF_MAX_SIGNIFICAND_DIGITS_f64 17

typedef struct {
  uint64_t high;
  uint64_t low;
} qf_u128;

// parsing
uint64_t qf_nonnull(1, 4) qf_parse_u64_decimal(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end) {
  intptr_t i = start;
  uint64_t result = 0;
  while (i < str_size) {
    uint8_t digit = str[i] - '0';
    uint64_t new_result;
    bool did_overflow = __builtin_mul_overflow(result, 10, &new_result);
    did_overflow |= __builtin_add_overflow(new_result, digit, &new_result);
    if (digit >= 10 || did_overflow) break;
    result = new_result;
    i++;
  }
  *end = i;
  return result;
}
int64_t qf_nonnull(1, 4) qf_parse_i64_decimal(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end) {
  // sign
  bool negative = str[start] == '-';
  intptr_t i = negative || str[start] == '+' ? start + 1 : start;
  // value
  int64_t result = 0;
  while (i < str_size) {
    int8_t digit = str[i] - '0';
    digit = negative ? -digit : digit;
    int64_t new_result;
    bool did_overflow = __builtin_mul_overflow(result, 10, &new_result);
    did_overflow |= __builtin_add_overflow(new_result, digit, &new_result);
    if (digit >= 10 || did_overflow) break;
    result = new_result;
    i++;
  }
  *end = i;
  return result;
}
uint64_t qf_nonnull(1, 4) qf_parse_u64_hex(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end) {
  uint64_t result = 0;
  intptr_t i = start;
  while (i < str_size) {
    uint64_t digit = 16;
    char c = str[i];
    uint8_t decimal = (uint8_t)(c - '0');
    uint8_t hex = qf_min((uint8_t)(c - 'A'), (uint8_t)(c - 'a'));
    digit = decimal <= 9 ? decimal : hex + 10;
    uint64_t new_result;
    bool did_overflow = __builtin_mul_overflow(result, 16, &new_result);
    did_overflow |= __builtin_add_overflow(new_result, digit, &new_result);
    if (digit >= 16 || did_overflow) break;
    result = new_result;
    i++;
  }
  *end = i;
  return result;
}
uint64_t qf_nonnull(1, 4, 5) qf_parse_f64_significand(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end, int32_t *restrict exponent_10_ptr) {
  uint64_t significand_10 = 0;
  intptr_t i = start;
  // integer
  intptr_t non_leading_zero_digits = 0;
  while (i < str_size && str[i] == '0') {
    i++;
  }
  // TODO: maybe just check if did_overflow
  while (i < str_size && non_leading_zero_digits < QF_MAX_SIGNIFICAND_DIGITS_f64) {
    uint8_t digit = str[i] - '0';
    if (digit >= 10) break;
    significand_10 = significand_10 * 10 + digit;
    non_leading_zero_digits++;
    i++;
  }
  // fraction
  // TODO: maybe SWAR?
  int32_t exponent_base10 = 0;
  if (qf_near(i < str_size && str[i] == '.')) {
    i++;
    if (qf_near(significand_10 == 0)) {
      while (i < str_size && str[i] == '0') {
        exponent_base10--;
        i++;
      }
    }
    while (i < str_size && non_leading_zero_digits < QF_MAX_SIGNIFICAND_DIGITS_f64) {
      uint8_t digit = str[i] - '0';
      if (digit >= 10) break;
      significand_10 = significand_10 * 10 + digit;
      non_leading_zero_digits++;
      exponent_base10--;
      i++;
    }
  }
  // ignore trailing noise (technically incorrect, but would require big ints)
  while (i < str_size && (str[i] - '0' <= 9)) {
    i++;
  }
  *end = i;
  *exponent_10_ptr = exponent_base10;
  return significand_10;
}
// Number Parsing at a Gigabyte per Second (Lemire 2022) https://arxiv.org/pdf/2101.11408
// TODO: Fast Number Parsing Without Fallback (Lemire 2023) https://arxiv.org/pdf/2212.06644
/* NOTE: MAX_SAFE_INTEGER_xx = 2^EXPLICIT_MANTISSA_BITS_xx - 1 */
#define MAX_SAFE_INTEGER_f64      9007199254740991
#define MAX_SAFE_POWER_OF_TEN_f64 22
const qf_f64 SAFE_POWERS_OF_TEN_f64[1 + MAX_SAFE_POWER_OF_TEN_f64] = {
  1e0,
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
qf_f64 qf_parse_f64(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end) {
  int32_t exponent_10;
  intptr_t i = start;
  bool negative = false;
  if (i < str_size && str[i] == '-') {
    i++;
    negative = true;
  }
  // TODO: parse inf/nan
  uint64_t significand_10 = qf_parse_f64_significand(str, str_size, i, &i, &exponent_10);
  if (str[i] == 'e') {
    i++;
    intptr_t prev_i = i;
    exponent_10 += qf_parse_i64_decimal(str, str_size, i, &i);
    if (qf_far(i == prev_i)) {
      // invalid exponent
      *end = start;
      return 0.0;
    }
  }
  qf_f64 value;
  if (qf_near((exponent_10 == 0) || (qf_abs(exponent_10) <= MAX_SAFE_POWER_OF_TEN_f64 && significand_10 <= MAX_SAFE_INTEGER_f64))) {
    // fast path
    if (qf_near(exponent_10 < 0)) {
      value = (qf_f64)significand_10 / SAFE_POWERS_OF_TEN_f64[-exponent_10];
    } else {
      value = (qf_f64)significand_10 * SAFE_POWERS_OF_TEN_f64[exponent_10];
    }
  } else {
    // general case
    if (significand_10 == 0 || exponent_10 < -342) {
      value = 0;
    } else if (exponent_10 > 308) {
      value = (qf_f64)1.0 / 0.0;
    } else {
      uint64_t I = qf_count_leading_zeros(significand_10);
      significand_10 = significand_10 << I;
      // TODO: make a build system to generate tables...
      qf_u128 z = POW5_INVERSE[exponent_10 + 342] * significand_10;
      if (qf_far(z.high == -1 && z.low == -1 && (exponent_10 < -27 || exponent_10 > 55))) {
        value = (qf_f64)0.0 / 0.0;
      }
      uint64_t mantissa = z.high >> (64 - 54);
      uint64_t u = z.high >> 63;
    }
  }
  printf("%llu, %i, %.17f", significand_10, exponent_10, value);
  *end = i;
  return negative ? -value : value;
}

// formatting
void format_f64(char buffer[restrict 30], qf_f64 value) {
  // TODO: dragonbox
  uint64_t value_u64;
  qf_bitcopy(&value, &value_u64);
  uint64_t sign = value_u64 >> 63;
  int64_t exponent = (int64_t)((value_u64 >> 52) & 0x7ff) - (0x7ff / 2);
  uint64_t mantissa = value_u64 & 0xfffffffffffff;
  // TODO: if nan_or_inf() {return ...}
  /* NOTE: everything  */
  if (qf_abs(value) < QF_CONCAT(1e, QF_MAX_SIGNIFICAND_DIGITS_f64)) {
    // print normally
  } else {
    // print in scientific notation
  }
  printf("%llu, %lli, %llx\n", sign, exponent, mantissa);
  // TODO: print sign
  // TODO: section 5.1
  // uint64_t range = ...;
  /*
  for (intptr_t = 0; i < 2; i++) {
    digits = mul_shift_mod(mantissa, table[exponent][i], e + c);
  }
  */
}
