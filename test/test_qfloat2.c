// ./run -crt
#include "lib/tests.h"
#include "lib/threads.h"
#define QFLOAT_NOLIBC !HAS_CRT
#include "../src/qfloat2.h"

void main_multicore(Thread t) {
  // TODO: fix race condition...
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
      check(t, group, parsed == test.out, hex, parsed);
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
    };
    for (intptr i = 0; i < countof(tests); i++) {
      Test test = tests[i];
      intptr end;
      i64 parsed = qf_parse_i64_decimal(test.in.ptr, (intptr)test.in.size, 0, &end);
      check(t, group, parsed == test.out, hex, parsed);
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
      check(t, group, parsed == test.out, hex, parsed);
    }
  }
  test_summary(t, group);
}
