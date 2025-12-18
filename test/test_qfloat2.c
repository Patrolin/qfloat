// ./run -crt
#include "lib/tests.h"
#include "lib/threads.h"
#define QFLOAT_NOLIBC !HAS_CRT
#include "../src/qfloat2.h"

void main_multicore(Thread t) {
  // qf_parse_u64_decimal()
  TestGroup* group;
  if (test_group(t, &group, string("qf_parse_u64_decimal()"), 1)) {
    TEST(string, u64);
    Test tests[] = {
        {string("0"), 0},
        {string("1"), 1},
        {string("18446744073709551615"), 18446744073709551615ULL},
    };
    for (intptr i = 0; i < countof(tests); i++) {
      Test test = tests[i];
      intptr end;
      u64 parsed = qf_parse_u64_decimal(test.in.ptr, (intptr)test.in.size, 0, &end);
      check(t, group, parsed == test.out, u64, parsed);
    }
  }
  test_summary(t, group);
  // qf_parse_i64_decimal()
  if (test_group(t, &group, string("qf_parse_i64_decimal()"), 1)) {
    TEST(string, i64);
    Test tests[] = {
        {string("0"), 0},
        {string("1"), 1},
        {string("9223372036854775807"), 9223372036854775807},
        {string("-9223372036854775808"), (int64_t)9223372036854775808ULL},
        {string("-9223372036854775808"), -(int64_t)9223372036854775807ULL - 1},
    };
    for (intptr i = 0; i < countof(tests); i++) {
      Test test = tests[i];
      intptr end;
      i64 parsed = qf_parse_i64_decimal(test.in.ptr, (intptr)test.in.size, 0, &end);
      check(t, group, parsed == test.out, i64, parsed);
    }
  }
  test_summary(t, group);
  // qf_parse_u64_hex()
  if (test_group(t, &group, string("qf_parse_u64_hex()"), 1)) {
    TEST(string, u64);
    Test tests[] = {
        {string("0"), 0},
        {string("123456"), 0x123456},
        {string("987654"), 0x987654},
        {string("abcdef"), 0xABCDEF},
        {string("FEDCAB"), 0xFEDCAB},
        {string("FFFFFFFFFFFFFFFF"), 0xFFFFFFFFFFFFFFFF},
    };
    for (intptr i = 0; i < countof(tests); i++) {
      Test test = tests[i];
      intptr end;
      u64 parsed = qf_parse_u64_hex(test.in.ptr, (intptr)test.in.size, 0, &end);
      check(t, group, parsed == test.out, u64, parsed);
    }
  }
  test_summary(t, group);
  // qf_parse_f64_significand()
  if (test_group(t, &group, string("qf_parse_f64_significand()"), 1)) {
    TEST(string, u64);
    Test tests[] = {
        {string("0"), 0},
        {string(".0"), 0},
        {string("0."), 0},
        {string("0.0"), 0},
        {string("654321"), 654321},
        {string("123456789.01234567"), 12345678901234567},
    };
    for (intptr i = 0; i < countof(tests); i++) {
      Test test = tests[i];
      intptr end;
      intptr exponent_base10;
      u64 parsed = qf_parse_f64_significand(test.in.ptr, (intptr)test.in.size, 0, &end, &exponent_base10);
      check(t, group, parsed == test.out, u64, parsed);
    }
  }
  test_summary(t, group);
}
