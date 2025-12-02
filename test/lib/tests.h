#pragma once
#include "definitions.h"
#include "fmt.h"
#include "threads.h"

// NOTE: qrng from https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
/* NOTE: For integers, (any_odd_number*n) % any_power_of_two is guaranteed to hit every number.
   Evaluate `Round[2^64 / phi]; phi = (1+sqrt(5))/2` in wolfram alpha,
   then if it's even, add 1 to it.
*/
#define PRIME_u64 u64(11400714819323198487ULL)
u64 random_u64(u64 seed) {
  return seed * PRIME_u64;
}

void print_test_progress(Thread t, u64 current_run, u64 max_runs) {
  if (expect_small(t == 0)) {
    printf2(string("\x1b[2K\rtests: %/%"), u64, current_run, u64, max_runs);
  }
}
void test(bool condition, u64 current_run, u64 value, u64* succeeded_ptr) {
  if (expect_likely(condition)) {
    atomic_fetch_add(succeeded_ptr, 1);
  } else {
    printf2(string("\x1b[2K\rfailed: i: %, v: %\n"), u64, current_run, uhex_pad, value);
  }
}
void print_tests_done(Thread t, u64 succeeded, u64 max_runs) {
  if (expect_likely(t != 0)) {
    return;
  }
  if (succeeded == max_runs) {
    println(string, string("\x1b[2K\rAll tests passed!"));
  } else {
    printfln1(string("\x1b[2K\r% tests failed..."), u64, max_runs - succeeded);
  }
}
