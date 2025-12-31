#include "definitions.h"
#include "mem.h"

#define max(a, b) ((a) > (b) ? (a) : (b))

// Integer slice
typedef struct {
  /* NOTE: two's complement chunks */
  u64* chunks;
  u64 chunks_size;
} Integer;
#define integer_sign_extension(a) (a.chunks[a.chunks_size - 1] >> 63 ? u64(-1) : 0)
#define integer_get_chunk(a, i, sign_extension) (i < a.chunks_size ? a.chunks[i] : sign_extension)
#define integer_alloc(chunks_size) {arena_alloc_array(global_arena, u64, chunks_size), chunks_size}
Integer integer_alloc_sign_extend(Integer a, u64 new_chunks_size) {
  if (expect_unlikely(a.chunks_size >= new_chunks_size)) return a;
  Integer result = integer_alloc(new_chunks_size);
  /* NOTE: split into two loops, so that optimizer can unroll them */
  intptr i = 0;
  do {
    result.chunks[i] = a.chunks[i];
  } while (++i < a.chunks_size);
  u64 sign_extension = integer_sign_extension(a);
  while (i < new_chunks_size) {
    result.chunks[i++] = sign_extension;
  }
  return result;
}
Integer integer_alloc_not(Integer a) {
  Integer result = integer_alloc(a.chunks_size);
  intptr i = 0;
  u64 carry = 1;
  do {
    result.chunks[i] = ~a.chunks[i];
  } while (++i < a.chunks_size);
  return result;
}
Integer integer_alloc_negate(Integer a) {
  /* NOTE: negate can overflow in two's complement... */
  Integer result = integer_alloc(a.chunks_size + 1);
  intptr i = 0;
  u64 carry = 1;
  do {
    /* NOTE: add_overflow() cannot be unrolled */
    carry = (u64)add_overflow(~a.chunks[i], carry, &result.chunks[i]);
  } while (++i < a.chunks_size);
  // assert(carry == 0);
  return result;
}
Integer integer_alloc_add(Integer a, Integer b) {
  u64 max_chunks_size = max(a.chunks_size, b.chunks_size) + 1;
  Integer result = integer_alloc_sign_extend(a, max_chunks_size);
  intptr i = 0;
  u64 carry = 0;
  u64 sign_extension = integer_sign_extension(b);
  do {
    /* NOTE: add_with_carry() cannot be unrolled */
    u64 b_chunk = integer_get_chunk(b, i, sign_extension);
    result.chunks[i] = add_with_carry(result.chunks[i], b_chunk, carry, &carry);
  } while (++i < max_chunks_size);
  return result;
}
Integer integer_alloc_sub(Integer a, Integer b) {
  u64 max_chunks_size = max(a.chunks_size, b.chunks_size) + 1;
  Integer result = integer_alloc_sign_extend(a, max_chunks_size);
  intptr i = 0;
  u64 borrow = 0;
  u64 sign_extension = integer_sign_extension(b);
  do {
    /* NOTE: sub_with_borrow() cannot be unrolled */
    u64 b_chunk = integer_get_chunk(b, i, sign_extension);
    result.chunks[i] = sub_with_borrow(result.chunks[i], b_chunk, borrow, &borrow);
  } while (++i < max_chunks_size);
  return result;
}
Integer _integer_alloc_karatsuba_mul(Integer a, Integer b) {
  // assert(a.chunks_size == b.chunks_size && a.chunks_size != 0 && a.chunks_size is a power of two)
  if (expect_likely(a.chunks_size == 1)) {
    __uint128_t result_128 = (__uint128_t)a.chunks[0] * (__uint128_t)b.chunks[0];
    Integer result = integer_alloc(2);
    result.chunks[0] = result_128 & (u64)-1;
    result.chunks[1] = u64(result_128 >> 64);
    return result;
  } else {
    intptr split = a.chunks_size / 2;
    Integer A = (Integer){a.chunks + split, u64(split)};
    Integer C = (Integer){a.chunks, u64(split)};
    Integer B = (Integer){b.chunks + split, u64(split)};
    Integer D = (Integer){b.chunks, u64(split)};
    // TODO: alloc temporaries on the stack instead
    Integer result_0 = _integer_alloc_karatsuba_mul(C, D);
    Integer result_2 = _integer_alloc_karatsuba_mul(A, B);
    Integer result_1 = _integer_alloc_karatsuba_mul(integer_alloc_add(A, C), integer_alloc_add(B, D));
    result_1 = integer_alloc_sub(result_1, result_0);
    result_1 = integer_alloc_sub(result_1, result_2);
    // merge result_0
    Integer result = integer_alloc_sign_extend(result_0, a.chunks_size * 2);
    // merge result_1
    intptr i = split;
    u64 carry = 0;
    u64 sign_extension = integer_sign_extension(result_1);
    do {
      u64 result_1_chunk = integer_get_chunk(result_1, i - split, sign_extension);
      result.chunks[i] = add_with_carry(result.chunks[i], result_1_chunk, carry, &carry);
    } while (++i < result.chunks_size);
    // merge result_2
    i = split * 2;
    carry = 0;
    sign_extension = integer_sign_extension(result_2);
    do {
      u64 result_2_chunk = integer_get_chunk(result_2, i - split * 2, sign_extension);
      result.chunks[i] = add_with_carry(result.chunks[i], result_2_chunk, carry, &carry);
    } while (++i < result.chunks_size);
    return result;
  }
}
Integer integer_alloc_mul(Integer a, Integer b) {
  u64 max_chunks_size = a.chunks_size + b.chunks_size;
  if (count_ones(u64, max_chunks_size) > 1) {
    max_chunks_size = 1 << (64 - count_leading_zeros(u64, max_chunks_size));
  }
  Integer left = integer_alloc_sign_extend(a, max_chunks_size);
  Integer right = integer_alloc_sign_extend(b, max_chunks_size);
  return _integer_alloc_karatsuba_mul(left, right);
}
Integer* integer_alloc_div(Integer* restrict a, Integer* restrict b) {
  /* TODO:
  - alloc(a) x2
  - make b have opposite sign to a
  - do long division
  */
  return 0;
}

// Rational
typedef struct {
  Integer* a;
  Integer* b;
} Rational;
