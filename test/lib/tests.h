#pragma once
#include "definitions.h"
#include "fmt.h"
#include "mem.h"
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

typedef struct {
  string name;
  u64 test_count;
  u64 fail_count;
} TestGroup;
bool NONNULL(2) test_group(Thread t, TestGroup** group, string name, Thread thread_count) {
  if (expect_small(t == 0)) {
    *group = arena_alloc(global_arena, TestGroup);
    **group = (TestGroup){.name = name};
  }
  barrier_scatter(t, group);
  return thread_count == 0 || t < thread_count;
}
void test_summary(Thread t, TestGroup* group) {
  barrier(t); /* NOTE: wait for writes */
  if (expect_small(t == 0)) {
    u64 test_count = group->test_count;
    u64 pass_count = test_count - group->fail_count;
    printf3(string(DELETE_LINE "   %: %/% tests passed\n"), string, group->name, u64, pass_count, u64, test_count);
  }
  barrier(t); /* NOTE: wait for reads */
}

// #define Test(t1, t2) Test_##t1##_##t2
#define Test(t1, t2) Test
#define TEST(t1, t2) \
  typedef struct {   \
    t1 in;           \
    t2 out;          \
  } Test(t1, t2)
#define check(thread, group, condition, t1, v1) check_impl(__COUNTER__, thread, group, condition, t1, v1)
#define check_impl(C, thread, group, condition, t1, v1) ({                                                          \
  u64 VAR(test_count, C) = atomic_add_fetch(&group->test_count, 1);                                                 \
  if (expect_small(t == 0)) {                                                                                       \
    u64 VAR(pass_count, C) = VAR(test_count, C) - group->fail_count;                                                \
    printf3(string(DELETE_LINE "  %: %/%"), string, group->name, u64, VAR(pass_count, C), u64, VAR(test_count, C)); \
  }                                                                                                                 \
  if (expect_unlikely(!(condition))) {                                                                              \
    u64 fail_count = atomic_add_fetch(&group->fail_count, 1);                                                       \
    printf3(string(DELETE_LINE "  %: test failed for % (%)\n"), string, group->name, hex, v1, t1, v1);              \
    abort();                                                                                                        \
  }                                                                                                                 \
})
