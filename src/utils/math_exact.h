#include "definitions.h"
#include "mem.h"
#include "process.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

// Integer
typedef struct {
  /* NOTE: two's complement */
  u64 digits_size;
  u64 digits[];
} Integer;
Integer* integer_alloc_negate(Integer* restrict a) {
  Integer* result = arena_alloc_flexible(global_arena, Integer, u64, a->digits_size);
  result->digits_size = a->digits_size;
  intptr i = 0;
  u64 carry = 1;
  do {
    carry = (u64)add_overflow(~a->digits[i], carry, &result->digits[i]);
  } while (++i < a->digits_size);
  return result;
}
Integer* integer_alloc_sign_extend(Integer* restrict a, u64 new_digits_size) {
  if (expect_unlikely(a->digits_size <= new_digits_size)) return a;
  Integer* result = arena_alloc_flexible(global_arena, Integer, u64, new_digits_size);
  result->digits_size = new_digits_size;
  intptr i = 0;
  do {
    result->digits[i] = a->digits[i];
  } while (++i < a->digits_size);
  u64 sign_extension = result->digits[i - 1] > MAX(i64) ? u64(-1) : 0;
  do {
    result->digits[i] = sign_extension;
  } while (++i < new_digits_size);
  return result;
}
Integer* integer_alloc_add(Integer* restrict a, Integer* restrict b) {
  u64 max_digits_size = max(a->digits_size, b->digits_size) + 1;
  Integer* left = integer_alloc_sign_extend(a, max_digits_size);
  Integer* right = integer_alloc_sign_extend(b, max_digits_size);
  intptr i = 0;
  u64 carry = 0;
  do {
    left->digits[i] = add_with_carry(a->digits[i], b->digits[i], carry, &carry);
  } while (++i < max_digits_size);
  return left;
}
Integer* integer_alloc_mul(Integer* restrict b) {
  /* TODO:
  - alloc(a + b)
  - sign extend up to that amount
  - mul
  */
  return 0;
}

// Rational
typedef struct {
  Integer* a;
  Integer* b;
} Rational;
