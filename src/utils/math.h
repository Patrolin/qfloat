#pragma once
#include "builtin.h"

// https://gcc.gnu.org/onlinedocs/gcc/Library-Builtins.html
#define NaN      __builtin_nan("")
#define isnan(x) __builtin_isnan(x)
#define isinf(x) __builtin_isinf(x)
#define absg(x)  ((x) < 0 ? (-(x)) : (x))
f64 trunc(f64 x) {
  f64 result;
#if ARCH_X64 || ARCH_X86
  asm("vroundsd %0, %1, %1, $11" : "=x"(result) : "x"(x));
#else
  assert(false);
#endif
  return result;
}
f64 floor(f64 x) {
  f64 result;
#if ARCH_X64 || ARCH_X86
  asm("vroundsd %0, %1, %1, $9" : "=x"(result) : "x"(x));
#else
  assert(false);
#endif
  return result;
}
/* NOTE: libc's `fmod()` returns the `remainder()`... */
#define remainder(a, b) ((a) - trunc((a) / (b)) * b)
#define modulo(a, b)    ((a) - floor((a) / (b)) * b)
#define sin(x)          __builtin_csin(x)
#define cos(x)          __builtin_ccos(x)

// random
/* NOTE: qRNG from https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
  Evaluate `1/ phi; phi = (1+sqrt(5))/2` in wolfram alpha
  then round to 17 digits (including the leading zeros!). */
#define PHI          1.6180339887498948
#define ONE_OVER_PHI 0.6180339887498948
f64 random(f64 prev) {
  return remainder((prev + ONE_OVER_PHI), 1.0);
}
/* NOTE: For integers, (any_odd_number*n) % any_power_of_two is guaranteed to hit every number.
   Evaluate `Round[2^64 / phi]; phi = (1+sqrt(5))/2` in wolfram alpha,
   then if it's even, add 1 to it. */
#define MAX_U64_OVER_PHI 11400714819323198487ULL
u64 random_u64(u64 prev) {
  return prev + MAX_U64_OVER_PHI;
}
u64 noise_u64(u64 position) {
  return position * MAX_U64_OVER_PHI;
}
