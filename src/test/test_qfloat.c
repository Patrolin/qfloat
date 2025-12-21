// ./run -crt
#include "lib/tests.h"
#include "lib/threads.h"
#include "../qfloat.h"
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
#ifdef SINGLE_CORE
  if (single_core(t)) {
#endif
    while (true) {
      current_run = atomic_fetch_add(runs_ptr, 1);
      if (expect_exit(current_run >= max_runs)) break;
      u64 value = random_u64(current_run);
#ifndef NDEBUG
      printfln1(string("\nv: %"), hex, value);
#endif
      bool ok = true;
      f64 value_f64 = bitcast(value, u64, f64);
      if (fabs(value_f64) < 1e-303) {
        test(ok, current_run, value, succeeded_ptr);
        continue;
      }
      // test qfloat_sprint_f64_libc()
      byte debug_buffer[QFLOAT_SIZE_f64];
      int debug_size = sprintf(debug_buffer, "%.17g", value_f64);
      byte buffer[QFLOAT_SIZE_f64];
      intptr size;
      f64 new_value;
      size = qfloat_sprint_f64_libc(value_f64, buffer);
      intptr _end;
      new_value = qfloat_str_to_f64_libc(buffer, size, 0, &_end);
      ok &= new_value == value_f64 || isnan(value_f64);
      if (expect_small(!ok)) {
        string s1 = (string){debug_buffer, Size(debug_size)};
        string s2 = (string){buffer, Size(size)};
        printfln4(string("\n% -> %  // libc\n% -> %  // libc shortened"), string, s1, hex, value, string, s2, hex, bitcast(new_value, f64, u64));
        assert(false);
      }
      // test sprint_f64()
      memset(buffer, 0, QFLOAT_SIZE_f64);
      size = sprint_f64(value_f64, buffer);
      new_value = str_to_f64(buffer, size, 0, &_end);
      ok &= new_value == value_f64 || isnan(value_f64);
      if (expect_small(!ok)) {
        string s1 = (string){debug_buffer, Size(debug_size)};
        string s2 = (string){buffer, Size(size)};
        printfln4(string("\n% -> %  // libc\n% -> %  // nolibc shortened"), string, s1, hex, value, string, s2, hex, bitcast(new_value, f64, u64));
        assert(false);
      }
      // test sprint_f64() x strtod()
      new_value = qfloat_str_to_f64_libc(buffer, size, 0, &_end);
      ok &= new_value == value_f64 || isnan(value_f64);
      if (expect_small(!ok)) {
        printfln1(string("\nv: %"), hex, value);
        string s1 = (string){debug_buffer, Size(debug_size)};
        string s2 = (string){buffer, Size(size)};
        printfln4(string("\n% -> %  // libc\n% -> %  // nolibc shortened x strtod()"), string, s1, hex, value, string, s2, hex, bitcast(new_value, f64, u64));
        assert(false);
      }
      test(ok, current_run, value, succeeded_ptr);
      // print progress
      print_test_progress(t == 0, current_run, max_runs);
    }
#ifdef SINGLE_CORE
  }
#endif
  barrier(t);

  print_tests_done(t, atomic_load(succeeded_ptr), max_runs);
}
