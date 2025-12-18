// https://github.com/Patrolin/qfloat
#pragma once
#include <stdint.h>
#include <stdbool.h>

// common
#if defined(__cplusplus) || (defined(_MSC_VER) && !defined(__clang__))
  #define QF_ASSERT(condition) static_assert(condition, #condition)
#else
  #define QF_ASSERT(condition) _Static_assert(condition, #condition)
#endif

/* NOTE: QF_SIGNIFICAND_DIGITS_xx = Math.ceil(EXPLICIT_MANTISSA_BITS_xx * Math.log10(2)) */
#define QF_SIGNIFICAND_DIGITS_f64 17
#define QF_SIGNIFICAND_DIGITS_f32 7
#define QF_SIGNIFICAND_DIGITS_f16 4
QF_ASSERT(sizeof(double) == 8);
QF_ASSERT(sizeof(float) == 4);
QF_ASSERT(sizeof(char) == 1);

#ifndef qf_assert
  #include <assert.h>
  #define qf_assert(condition) assert(condition)
#endif
#if defined(__clang__) || defined(__GNUC__)
  #define qf_nonnull(...) __attribute__((nonnull(__VA_ARGS__)))
  #define qf_likely(condition) __builtin_expect(condition, true)
  #define qf_unlikely(condition) __builtin_expect(condition, false)
#else
  #define qf_nonnull(...)
  #define qf_likely(condition) (condition)
  #define qf_unlikely(condition) (condition)
  #include <intrin.h> /* NOTE: required for overflow in MSVC */
#endif
/* NOTE: When there aren't any `break` or `return` statements, jump over the if block. */
#define qf_small(condition) qf_likely(condition)
/* NOTE: When there are `break` or `return` statements, let the compiler decide.
  X64 has a variable instruction size, so qf_likely() produces more instructions, but less bytecode,
  on other architectures, qf_likely() probably produces more bytecode. */
#define qf_exit(condition) (condition)

#define qf_min(a, b) ((a) < (b) ? (a) : (b))
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
uint64_t qf_nonnull(1, 4, 5) qf_parse_f64_significand(const char *restrict str, intptr_t str_size, intptr_t start, intptr_t *restrict end, intptr_t *restrict exponent_base10_ptr) {
  uint64_t result = 0;
  intptr_t i = start;
  // integer
  intptr_t non_leading_zero_digits = 0;
  while (i < str_size && str[i] == '0') {
    i++;
  }
  while (i < str_size && non_leading_zero_digits < QF_SIGNIFICAND_DIGITS_f64) {
    uint8_t digit = str[i] - '0';
    if (qf_exit(digit >= 10)) break;
    result = result * 10 + digit;
    non_leading_zero_digits++;
    i++;
  }
  // fraction
  intptr_t exponent_base10 = 0;
  if (qf_small(i < str_size && str[i] == '.')) {
    i++;
    if (qf_small(result == 0)) {
      while (i < str_size && str[i] == '0') {
        exponent_base10--;
        i++;
      }
    }
    while (i < str_size && non_leading_zero_digits < QF_SIGNIFICAND_DIGITS_f64) {
      uint8_t digit = str[i] - '0';
      if (qf_exit(digit >= 10)) break;
      result = result * 10 + digit;
      non_leading_zero_digits++;
      exponent_base10--;
      i++;
    }
  }
  *end = i;
  *exponent_base10_ptr = exponent_base10;
  return result;
}
