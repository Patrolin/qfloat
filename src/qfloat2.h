// https://github.com/Patrolin/qfloat
#pragma once
#include <stdint.h>
#include <stdbool.h>

// common
#if defined(__cplusplus)
  #define QF_ASSERT(condition) static_assert(condition, #condition)
#else
  #define QF_ASSERT(condition) _Static_assert(condition, #condition)
#endif
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
#define qf_min(a, b) ((a < b) ? (a) : (b))

bool qf_nonnull(3) qf_add_overflow_u64(uint64_t a, uint64_t b, uint64_t *result_ptr) {
  /* NOTE: overflows on the 65th bit */
#if defined(__clang__) || defined(__GNUC__)
  uint64_t result;
  bool overflow = __builtin_add_overflow(a, b, &result);
#else
  uint64_t result = a + b;
  bool overflow = result < a; /* NOTE: only works for adding/subtracting unsigned integers */
#endif
  if (qf_likely(!overflow)) {
    *result_ptr = result;
  }
  return overflow;
}
bool qf_nonnull(3) qf_add_overflow_i64(int64_t a, int64_t b, int64_t *result_ptr) {
  /* NOTE: overflows on the 64th bit */
#if defined(__clang__) || defined(__GNUC__)
  int64_t result;
  bool overflow = __builtin_add_overflow(a, b, &result);
#else
  int64_t result = a + b;
  bool overflow = ((a ^ result) & (b ^ result)) < 0;
#endif
  if (qf_likely(!overflow)) {
    *result_ptr = result;
  }
  return overflow;
}
// bool qf_nonnull(3) qf_sub_overflow_i64(int64_t a, int64_t b, int64_t *result_ptr) {
// #if defined(__clang__) || defined(__GNUC__)
//   /* NOTE: clang/gcc emit `neg; add` instead of `sub` unless we use the intrinsic... */
//   int64_t result;
//   bool overflow = __builtin_sub_overflow(a, b, &result);
//   if (qf_likely(!overflow)) {
//     *result_ptr = result;
//   }
//   return overflow;
// #else
//   /* NOTE: MSVC doesn't have a subtract overflow intrinsic, but it inlines and optimizes to a `sub` instruction */
//   return qf_add_overflow_i64(a, -b, result_ptr);
// #endif
// }
bool qf_nonnull(3) qf_mul_overflow_u64(uint64_t a, uint64_t b, uint64_t *result_ptr) {
#if defined(__clang__) || defined(__GNUC__)
  uint64_t result;
  bool overflow = __builtin_mul_overflow(a, b, &result);
#else
  uint64_t result_high;
  uint64_t result = _umul128(a, b, &result_high);
  bool overflow = result_high != 0;
#endif
  if (qf_likely(!overflow)) {
    *result_ptr = result;
  }
  return overflow;
}
bool qf_nonnull(3) qf_mul_overflow_i64(int64_t a, int64_t b, int64_t *result_ptr) {
#if defined(__clang__) || defined(__GNUC__)
  int64_t result;
  bool overflow = __builtin_mul_overflow(a, b, &result);
#else
  int64_t result_high;
  int64_t result = _mul128(a, b, &result_high);
  bool overflow = result_high != 0;
#endif
  if (qf_likely(!overflow)) {
    *result_ptr = result;
  }
  return overflow;
}

#define QF_BASE10_DIGITS_f64 17

// parsing // TODO: test these
uint64_t qf_nonnull(1, 4) qf_parse_u64_decimal(const char *str, intptr_t str_size, intptr_t start, intptr_t *end) {
  uint64_t result = 0;
  intptr_t i = start;
  while (i < str_size) {
    uint8_t digit = str[i] - '0';
    bool did_overflow = qf_mul_overflow_u64(result, 10, &result);
    did_overflow |= qf_add_overflow_u64(result, (uint64_t)digit, &result);
    if (digit >= 10 || did_overflow) break;
    i++;
  }
  *end = i;
  return result;
}
int64_t qf_nonnull(1, 4) qf_parse_i64_decimal(const char *str, intptr_t str_size, intptr_t start, intptr_t *end) {
  // sign
  bool negative = str[start] == '-';
  intptr_t i = negative || str[start] == '+' ? start + 1 : start;
  // value
  int64_t result = 0;
  while (i < str_size) {
    int64_t digit = (int64_t)(str[i] - '0');
    bool did_overflow = qf_mul_overflow_i64(result, 10, &result);
    digit = negative ? -digit : digit;
    did_overflow |= qf_add_overflow_i64(result, digit, &result);
    if (digit >= 10 || did_overflow) break;
    i++;
  }
  *end = i;
  return result;
}
uint64_t qf_nonnull(1, 4) qf_parse_u64_hex(const char *str, intptr_t str_size, intptr_t start, intptr_t *end) {
  uint64_t result = 0;
  intptr_t i = start;
  while (i < str_size) {
    uint64_t digit = 16;
    char c = str[i];
    char decimal = c - '0';
    if (decimal <= '9' - '0') {
      digit = decimal;
    } else {
      char uppercase_hex = c - 'A';
      char lowercase_hex = c - 'a';
      char hex = qf_min(uppercase_hex, lowercase_hex);
      if (hex <= 'F' - 'A') {
        digit = hex + 10;
      }
    }
    bool did_overflow = qf_mul_overflow_u64(result, 16, &result);
    did_overflow |= qf_add_overflow_u64(result, digit, &result);
    if (digit >= 16 || did_overflow) break;
    i++;
  }
  *end = i;
  return result;
}
uint64_t qf_nonnull(1, 4, 5) qf_parse_f64_significand(const char *str, intptr_t str_size, intptr_t start, intptr_t *end, intptr_t *exponent_offset_ptr) {
  uint64_t result = 0;
  intptr_t i = start;
  // integer
  intptr_t non_leading_zero_digits = 0;
  while (i < str_size && str[i] == '0') {
    i++;
  }
  while (i < str_size && non_leading_zero_digits < QF_BASE10_DIGITS_f64) {
    uint8_t digit = str[i] - '0';
    uint64_t new_result = result * 10 + digit;
    if (digit >= 10) break;
    result = new_result;
    non_leading_zero_digits++;
    i++;
  }
  // fraction
  intptr_t exponent_offset = 0;
  if (i < str_size && str[i] == '.') {
    i++;
    if (result == 0) {
      while (i < str_size && str[i] == '0') {
        exponent_offset--;
        i++;
      }
    }
    while (i < str_size && non_leading_zero_digits < QF_BASE10_DIGITS_f64) {
      uint8_t digit = str[i] - '0';
      uint64_t new_result = result * 10 + digit;
      if (digit >= 10) break;
      result = new_result;
      non_leading_zero_digits++;
      exponent_offset--;
      i++;
    }
  }
  *end = i;
  *exponent_offset_ptr = exponent_offset;
  return result;
}
