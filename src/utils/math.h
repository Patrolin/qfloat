#pragma once
#include "builtin.h"

// float
#define NaN __builtin_nan("")
/* remainder(a, b), modulo(a, b) for |a/b| < MAX_SAFE_INTEGER, result is undefined if `b == 0.0`
  NOTE: libc's `fmod()` returns the `remainder()`... */
double remainder(double a, double b) {
  i64 q_int = (i64)(a / b); // truncate towards zero
  return a - (double)q_int * b;
}
double modulo(double a, double b) {
  double q = a / b;
  i64 q_int = (i64)q;                         // truncate towards zero
  if (q < 0.0 && q != (double)q_int) q_int--; // fixup
  return a - (double)q_int * b;
}

// random
/* NOTE: qrng from https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
  Evaluate `1/ phi; phi = (1+sqrt(5))/2` in wolfram alpha
  then round to 17 digits. */
#define PHI          1.6180339887498948
#define ONE_OVER_PHI 0.61803398874989484
double random(double prev) {
  return remainder((prev + ONE_OVER_PHI), 1.0);
}
/* NOTE: For integers, (any_odd_number*n) % any_power_of_two is guaranteed to hit every number.
   Evaluate `Round[2^64 / phi]; phi = (1+sqrt(5))/2` in wolfram alpha,
   then if it's even, add 1 to it. */
u64 random_u64(u64 seed) {
  return seed * 11400714819323198487ULL;
}
