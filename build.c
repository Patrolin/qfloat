// clang build.c -o build.exe && ./build.exe
#pragma push_macro("SINGLE_CORE")
#pragma push_macro("NASSERT")
#define SINGLE_CORE 1
#include "src/test/lib/process.h"
#pragma pop_macro("NASSERT")
#pragma pop_macro("SINGLE_CORE")

#define LIB_CHARCONV_INPUT "src/test/alternatives/lib_charconv.cpp"
#define LIB_CHARCONV_OUTPUT "generated/charconv.dll"
#define TESTS_INPUT "src/test/test_qfloat2.c"
#define TESTS_OUTPUT "test_qfloat.exe"

void build_lib_charconv();
void run_tests();
void main_singlecore() {
  build_lib_charconv();
  run_tests();
}

void build_lib_charconv() {
  BuildArgs args = {};
  alloc_arg(&args, LIB_CHARCONV_INPUT);
  alloc_arg2(&args, "-o", LIB_CHARCONV_OUTPUT);
  alloc_arg(&args, "-shared");
  alloc_arg(&args, "-std=c++17");
  alloc_arg(&args, "-O2");
  run_process("clang", &args);
}
void run_tests() {
  BuildArgs args = {};
  // input
  alloc_arg(&args, TESTS_INPUT);
  alloc_arg2(&args, "-o", TESTS_OUTPUT);
  // c standard
  alloc_arg(&args, "-march=native");
  alloc_arg(&args, "-masm=intel");
  alloc_arg(&args, "-std=gnu99");
  alloc_arg(&args, "-fno-signed-char");
  alloc_arg(&args, "-Werror");
  alloc_arg(&args, "-Wconversion");
  alloc_arg(&args, "-Wsign-conversion");
  alloc_arg(&args, "-Wnullable-to-nonnull-conversion");
  // linker
  alloc_arg(&args, "-fuse-ld=lld");
#if OS_WINDOWS
  alloc_arg(&args, "-Wl,/STACK:0x100000");
#elif OS_LINUX
  /* NOTE: linux forces it's own stack size on you, which you can only reduce */
#endif
  // params
#if OPT
  alloc_arg(&args, "-O2");
  alloc_arg(&args, "-flto");
  alloc_arg(&args, "-g");
#else
  alloc_arg(&args, "-O0");
  alloc_arg(&args, "-g");
#endif
#if NOLIBC
  alloc_arg(&args, "-nostdlib");
  alloc_arg(&args, "-fno-builtin");
  alloc_arg(&args, "-mno-stack-arg-probe");
  alloc_arg(&args, "-DNOLIBC");
#endif
#if SINGLE_CORE
  alloc_arg(&args, "-DSINGLE_CORE");
#endif
#if NASSERT
  alloc_arg(&args, "-DNASSERT");
#endif
#if NDEBUG
  alloc_arg(&args, "-DNDEBUG");
#endif
  // compile
  // TODO: delete .pdb, .rdi?
  run_process("clang", &args);
  // run
  run_process(TESTS_OUTPUT, 0);
}
