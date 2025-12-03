// ./run -crt
#include "lib/all.h"
#define QFLOAT_NOLIBC !HAS_CRT
#include "../src/qfloat.h"
#include <stdio.h>

void main_multicore(Thread t) {
  u64* runs_ptr;
  u64* succeeded_ptr;
  if (single_core(t)) {
    runs_ptr = arena_alloc(global_arena, u64);
    succeeded_ptr = arena_alloc(global_arena, u64);
  }
  barrier_scatter(t, &runs_ptr);
  barrier_scatter(t, &succeeded_ptr);

  u64 current_run;
  u64 max_runs = u64(MAX(u32));
  while (true) {
    current_run = atomic_fetch_add(runs_ptr, 1);
    if (expect_small(current_run >= max_runs)) break;
    u64 value = random_u64(current_run);
    bool ok = true;
    f64 value_f64 = bitcast(value, u64, f64);
    // test qfloat_sprint_f64_libc()
    byte debug_buffer[QFLOAT_SIZE_f64];
    byte buffer[QFLOAT_SIZE_f64];
    intptr size;
    f64 new_value;
#if HAS_CRT
    size = qfloat_sprint_f64_libc(value_f64, buffer);
    new_value = qfloat_str_to_f64_libc(buffer, size);
    ok &= new_value == value_f64 || isnan(value_f64);
    if (expect_small(!ok)) {
      int debug_size = sprintf(debug_buffer, "%.17g", value_f64);
      string s1 = (string){debug_buffer, Size(debug_size)};
      string s2 = (string){buffer, Size(size)};
      printfln4(string("\n% -> %  // libc\n% -> %  // libc shortened"), string, s1, uhex, value, string, s2, uhex, bitcast(new_value, f64, u64));
      assert(false);
    }
#endif
    // test sprint_f64()
    memset(buffer, 0, QFLOAT_SIZE_f64);
    size = sprint_f64(value_f64, buffer);
    new_value = str_to_f64(buffer, size);
    ok &= new_value == value_f64 || isnan(value_f64);
    if (expect_small(!ok)) {
      int libc_size = sprintf(debug_buffer, "%.17g", value_f64);
      string s1 = (string){debug_buffer, Size(libc_size)};
      string s2 = (string){buffer, Size(size)};
      printfln4(string("\n% -> %  // libc\n% -> %  // nolibc shortened"), string, s1, uhex, value, string, s2, uhex, bitcast(new_value, f64, u64));
      assert(false);
    }
    test(ok, current_run, value, succeeded_ptr);
    // print progress
    print_test_progress(t == 0, current_run, max_runs);
  }
  barrier(t);

  print_tests_done(t, atomic_load(succeeded_ptr), max_runs);
}
