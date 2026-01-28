#pragma once
#include "builtin.h"

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
#define U64_OVER_PHI 11400714819323198487ULL
u64 random_u64(u64 prev) {
  return prev + U64_OVER_PHI;
}
u64 noise_u64(u64 position) {
  return position * U64_OVER_PHI;
}
