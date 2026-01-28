#pragma once
#include "builtin.h"
#include "fmt.h"
#include "mem.h"
#include "threads.h"

STRUCT(TestGroup) {
  string name;
  u64 test_count;
  u64 fail_count;
};
bool nonnull_(2) test_group(Thread t, TestGroup **group, string name, Thread thread_count) {
  if (expect_near(t == 0)) {
    *group = arena_alloc(&global_arena, TestGroup);
    **group = (TestGroup){.name = name};
  }
  barrier_scatter(t, group);
  return thread_count == 0 || t < thread_count;
}
void test_summary(Thread t, TestGroup *group) {
  barrier(t); /* NOTE: wait for writes */
  if (expect_near(t == 0)) {
    u64 test_count = group->test_count;
    u64 pass_count = test_count - group->fail_count;
    printf3(string(DELETE_LINE "   %: %/% tests passed\n"), string, group->name, u64, pass_count, u64, test_count);
  }
  barrier(t); /* NOTE: wait for reads */
}

// #define Test(t1, t2) Test_##t1##_##t2
#define Test(t1, t2) Test
#define TEST(t1, t2)     \
  STRUCT(Test(t1, t2)) { \
    t1 in;               \
    t2 out;              \
  }
#define check(thread, group, condition, t1, v1)         check_impl(__COUNTER__, thread, group, condition, t1, v1)
#define check_impl(C, thread, group, condition, t1, v1) ({                                                          \
  u64 VAR(test_count, C) = atomic_add_fetch(&group->test_count, 1);                                                 \
  if (expect_near(t == 0)) {                                                                                        \
    u64 VAR(pass_count, C) = VAR(test_count, C) - group->fail_count;                                                \
    printf3(string(DELETE_LINE "  %: %/%"), string, group->name, u64, VAR(pass_count, C), u64, VAR(test_count, C)); \
  }                                                                                                                 \
  if (expect_far(!(condition))) {                                                                                   \
    u64 fail_count = atomic_add_fetch(&group->fail_count, 1);                                                       \
    printf3(string(DELETE_LINE "  %: test failed for % (%)\n"), string, group->name, hex, v1, t1, v1);              \
    abort();                                                                                                        \
  }                                                                                                                 \
})
