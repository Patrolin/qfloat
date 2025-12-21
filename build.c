// clang build.c -o build.exe && build.exe
#define SINGLE_CORE 1
#include "src/test/lib/process.h"

void main_singlecore() {
  BuildArgs args = {};
  alloc_arg(&args, "src/test/test_qfloat2.c");
  alloc_arg2(&args, "-o", "test_qfloat.exe");
  run_process("clang.exe", &args);
}
