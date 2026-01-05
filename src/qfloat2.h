// https://github.com/Patrolin/qfloat
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// common
#ifndef NOLIBC
  #include <assert.h>
  #include <string.h> /* NOTE: for memcpy() */
#endif
#ifndef qf_assert
  #define qf_assert(condition) assert(condition)
#endif
#ifndef QF_ASSERT
  #if defined(__cplusplus) || (defined(_MSC_VER) && !defined(__clang__))
    #define QF_ASSERT(condition) static_assert(condition, #condition)
  #else
    #define QF_ASSERT(condition) _Static_assert(condition, #condition)
  #endif
  // likely
  #if defined(__clang__) || defined(__GNUC__)
    #define qf_nonnull(...)        __attribute__((nonnull(__VA_ARGS__)))
    #define qf_likely(condition)   __builtin_expect(condition, true)
    #define qf_unlikely(condition) __builtin_expect(condition, false)
  #else
    #define qf_nonnull(...)
    #define qf_likely(condition)   (condition)
    #define qf_unlikely(condition) (condition)
    #include <intrin.h> /* NOTE: required for overflow in MSVC */
  #endif
  /* NOTE: When there aren't any `break` or `return` statements, jump over the if block. */
  #define qf_small(condition) qf_likely(condition)
  /* NOTE: When there are `break` or `return` statements, let the compiler decide.
    X64 has a variable instruction size, so qf_likely() produces more instructions, but less bytecode,
    on other architectures, qf_likely() probably produces more bytecode. */
  #define qf_exit(condition) (condition)
  // utils
  #define QF_CONCAT_IMPL(a, b) a##b
  #define QF_CONCAT(a, b)      QF_CONCAT_IMPL(a, b)
  #define qf_bitcopy(v1, v2)                   \
    QF_ASSERT(sizeof(*(v1)) == sizeof(*(v2))); \
    memcpy(v2, v1, sizeof(*(v1)));
  #define qf_abs(a)                 ((a) < 0 ? -(a) : (a))
  #define qf_min(a, b)              ((a) < (b) ? (a) : (b))
  #define qf_count_leading_zeros(a) ((typeof(a))__builtin_clzg(a))
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

/* NOTE: overflows on the 65th bit */
static bool qf_nonnull(3) qf_add_overflow_u64(uint64_t a, uint64_t b, uint64_t *restrict result_ptr) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_add_overflow(a, b, result_ptr);
#else
  uint64_t result = a + b;
  *result_ptr = result;
  return result < a; /* NOTE: only works for adding/subtracting unsigned integers */
#endif
}
/* NOTE: overflows on the 64th bit */
static bool qf_nonnull(3) qf_add_overflow_i64(int64_t a, int64_t b, int64_t *restrict result_ptr) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_add_overflow(a, b, result_ptr);
#else
  int64_t result = a + b;
  *result_ptr = result;
  return ((a ^ result) & (b ^ result)) < 0;
#endif
}
// bool qf_nonnull(3) qf_sub_overflow_i64(int64_t a, int64_t b, int64_t *restrict result_ptr) {
// #if defined(__clang__) || defined(__GNUC__)
//   /* NOTE: clang/gcc emit `neg; add` instead of `sub` unless we use the intrinsic... */
//   return __builtin_sub_overflow(a, b, result_ptr);
// #else
//   /* NOTE: MSVC doesn't have a subtract overflow intrinsic, but it inlines and optimizes to a `sub` instruction */
//   return qf_add_overflow_i64(a, -b, result_ptr);
// #endif
// }
static bool qf_nonnull(3) qf_mul_overflow_u64(uint64_t a, uint64_t b, uint64_t *restrict result_ptr) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_mul_overflow(a, b, result_ptr);
#else
  uint64_t result_high;
  uint64_t result = _umul128(a, b, &result_high);
  *result_ptr = result;
  return result_high != 0;
#endif
}
static bool qf_nonnull(3) qf_mul_overflow_i64(int64_t a, int64_t b, int64_t *restrict result_ptr) {
#if defined(__clang__) || defined(__GNUC__)
  return __builtin_mul_overflow(a, b, result_ptr);
#else
  int64_t result_high;
  int64_t result = _mul128(a, b, &result_high);
  *result_ptr = result;
  return result_high != 0;
#endif
}

// parsing
uint64_t qf_nonnull(1, 4) qf_parse_u64_decimal(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end) {
  intptr_t i = start;
  uint64_t result = 0;
  while (i < str_size) {
    uint8_t digit = str[i] - '0';
    uint64_t new_result;
    bool did_overflow = qf_mul_overflow_u64(result, 10, &new_result);
    did_overflow |= qf_add_overflow_u64(new_result, digit, &new_result);
    if (qf_exit(digit >= 10 || did_overflow)) break;
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
    bool did_overflow = qf_mul_overflow_i64(result, 10, &new_result);
    did_overflow |= qf_add_overflow_i64(new_result, digit, &new_result);
    if (qf_exit(digit >= 10 || did_overflow)) break;
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
    bool did_overflow = qf_mul_overflow_u64(result, 16, &new_result);
    did_overflow |= qf_add_overflow_u64(new_result, digit, &new_result);
    if (qf_exit(digit >= 16 || did_overflow)) break;
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
    if (qf_exit(digit >= 10)) break;
    significand_10 = significand_10 * 10 + digit;
    non_leading_zero_digits++;
    i++;
  }
  // fraction
  // TODO: maybe SWAR?
  int32_t exponent_base10 = 0;
  if (qf_small(i < str_size && str[i] == '.')) {
    i++;
    if (qf_small(significand_10 == 0)) {
      while (i < str_size && str[i] == '0') {
        exponent_base10--;
        i++;
      }
    }
    while (i < str_size && non_leading_zero_digits < QF_MAX_SIGNIFICAND_DIGITS_f64) {
      uint8_t digit = str[i] - '0';
      if (qf_exit(digit >= 10)) break;
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
    if (qf_unlikely(i == prev_i)) {
      // invalid exponent
      *end = start;
      return 0.0;
    }
  }
  qf_f64 value;
  if (qf_likely((exponent_10 == 0) || (qf_abs(exponent_10) <= MAX_SAFE_POWER_OF_TEN_f64 && significand_10 <= MAX_SAFE_INTEGER_f64))) {
    // fast path
    if (qf_likely(exponent_10 < 0)) {
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
      if (qf_unlikely(z.high == -1 && z.low == -1 && (exponent_10 < -27 || exponent_10 > 55))) {
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
