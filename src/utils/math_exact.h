#pragma once
#include "definitions.h"

// Integer slice
typedef struct {
  /* NOTE: two's complement chunks */
  u64 *chunks;
  u64 chunks_size;
} Integer;
#define integer_stack_alloc(size)               ((Integer){stack_alloc_array(u64, size), size})
#define integer_sign_extension(a)               (a.chunks[a.chunks_size - 1] >> 63 ? u64(-1) : 0)
#define integer_get_chunk(a, i, sign_extension) (i < a.chunks_size ? a.chunks[i] : sign_extension)

/* NOTE: we can't use `restrict`, as we sometimes want to do stuff like `x += y` */
void integer_sign_extend(Integer *result, Integer a) {
  /* NOTE: split into two loops, so that optimizer can unroll them */
  // copy
  intptr i = 0;
  u64 chunks_size = a.chunks_size;
  do {
    result->chunks[i] = a.chunks[i];
  } while (++i < chunks_size);
  // sign extend
  u64 extension = integer_sign_extension(a);
  chunks_size = result->chunks_size;
  while (i < chunks_size) {
    result->chunks[i++] = extension;
  }
}
#define integer_not_size(a) (a.chunks_size)
void integer_not(Integer *result, Integer a) {
  // not
  intptr i = 0;
  u64 carry = 1;
  u64 chunks_size = a.chunks_size;
  do {
    result->chunks[i] = ~a.chunks[i];
  } while (++i < chunks_size);
  // extend
  chunks_size = result->chunks_size;
  u64 extension = ~integer_sign_extension(a);
  while (i < chunks_size) {
    result->chunks[i++] = extension;
  }
}
/* NOTE: negate can overflow in two's complement... */
#define integer_negate_size(a) (a.chunks_size + 1)
void integer_negate(Integer *result, Integer a) {
  // negate
  intptr i = 0;
  u64 carry = 1;
  u64 chunks_size = a.chunks_size;
  do {
    /* NOTE: add_overflow() cannot be unrolled */
    carry = (u64)add_overflow(~a.chunks[i], carry, &result->chunks[i]);
  } while (++i < chunks_size);
  // assert(carry == 0);
  // extend
  chunks_size = result->chunks_size;
  u64 extension = ~integer_sign_extension(a);
  while (i < chunks_size) {
    result->chunks[i++] = extension;
  }
}
#define integer_add_size(a, b) (max(a.chunks_size, b.chunks_size) + 1)
void integer_add(Integer *result, Integer a, Integer b) {
  intptr i = 0;
  u64 carry = 0;
  u64 extension_a = integer_sign_extension(a);
  u64 extension_b = integer_sign_extension(b);
  u64 chunks_size = result->chunks_size;
  do {
    /* NOTE: add_with_carry() cannot be unrolled */
    u64 a_chunk = integer_get_chunk(a, i, extension_a);
    u64 b_chunk = integer_get_chunk(b, i, extension_b);
    result->chunks[i] = add_with_carry(a_chunk, b_chunk, carry, &carry);
  } while (++i < chunks_size);
}
#define integer_sub_size(a, b) integer_add_size(a, b)
void integer_sub(Integer *result, Integer a, Integer b) {
  intptr i = 0;
  u64 borrow = 0;
  u64 extension_a = integer_sign_extension(a);
  u64 extension_b = integer_sign_extension(b);
  u64 chunks_size = result->chunks_size;
  do {
    /* NOTE: sub_with_borrow() cannot be unrolled */
    u64 a_chunk = integer_get_chunk(a, i, extension_a);
    u64 b_chunk = integer_get_chunk(b, i, extension_b);
    result->chunks[i] = sub_with_borrow(a_chunk, b_chunk, borrow, &borrow);
  } while (++i < chunks_size);
}
#define _integer_karatsuba_mul_size(a, b) (a.chunks_size == 1 ? 2 : a.chunks_size * 2)
void _integer_karatsuba_mul(Integer *result, Integer a, Integer b) {
  // assert(a.chunks_size == b.chunks_size && a.chunks_size != 0 && a.chunks_size is a power of two)
  if (expect_likely(a.chunks_size == 1)) {
    __uint128_t result_128 = (__uint128_t)a.chunks[0] * (__uint128_t)b.chunks[0];
    result->chunks[0] = result_128 & (u64)-1;
    result->chunks[1] = u64(result_128 >> 64);
  } else {
    intptr split = a.chunks_size / 2;
    Integer A = (Integer){a.chunks + split, u64(split)};
    Integer C = (Integer){a.chunks, u64(split)};
    Integer B = (Integer){b.chunks + split, u64(split)};
    Integer D = (Integer){b.chunks, u64(split)};

    Integer result_0 = integer_stack_alloc(a.chunks_size);
    _integer_karatsuba_mul(&result_0, C, D);

    Integer result_2 = integer_stack_alloc(a.chunks_size);
    _integer_karatsuba_mul(&result_2, A, B);

    Integer result_ApC = integer_stack_alloc(u64(split) + 1);
    integer_add(&result_ApC, A, C);
    Integer result_BpD = integer_stack_alloc(u64(split) + 1);
    integer_add(&result_BpD, B, D);
    Integer result_1 = integer_stack_alloc((u64(split) + 1) * 2);
    _integer_karatsuba_mul(&result_1, result_ApC, result_BpD);
    integer_sub(&result_1, result_1, result_0);
    integer_sub(&result_1, result_1, result_2);
    // merge result_0
    integer_sign_extend(result, result_0);
    // merge result_1
    intptr i = split;
    u64 carry = 0;
    u64 extension = integer_sign_extension(result_1);
    u64 chunks_size = result->chunks_size;
    do {
      u64 result_1_chunk = integer_get_chunk(result_1, i - split, extension);
      result->chunks[i] = add_with_carry(result->chunks[i], result_1_chunk, carry, &carry);
    } while (++i < chunks_size);
    // merge result_2
    i = split * 2;
    carry = 0;
    extension = integer_sign_extension(result_2);
    do {
      u64 result_2_chunk = integer_get_chunk(result_2, i - split * 2, extension);
      result->chunks[i] = add_with_carry(result->chunks[i], result_2_chunk, carry, &carry);
    } while (++i < chunks_size);
  }
}
#define integer_mul_size(a, b)         integer_mul_size_impl(__COUNTER__, a, b)
#define integer_mul_size_impl(C, a, b) ({                                                    \
  u64 VAR(max_chunks_size, C) = a.chunks_size + b.chunks_size;                               \
  if (count_ones(u64, VAR(max_chunks_size, C)) > 1) {                                        \
    VAR(max_chunks_size, C) = 1 << (64 - count_leading_zeros(u64, VAR(max_chunks_size, C))); \
  }                                                                                          \
  VAR(max_chunks_size, C);                                                                   \
})
void integer_mul(Integer *result, u64 size, Integer a, Integer b) {
  Integer left = integer_stack_alloc(size);
  integer_sign_extend(&left, a);
  Integer right = integer_stack_alloc(size);
  integer_sign_extend(&right, b);
  _integer_karatsuba_mul(result, left, right);
}
Integer *integer_div(Integer *restrict a, Integer *restrict b) {
  /* TODO:
  - alloc(a) x2
  - make b have opposite sign to a
  - do long division
  */
  return 0;
}

// Rational
typedef struct {
  Integer *a;
  Integer *b;
} Rational;
