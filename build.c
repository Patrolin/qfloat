// clang src/build.c -o build.exe && ./build.exe
// clang build.c -o build.exe -DNOLIBC -masm=intel && ./build.exe
#pragma push_macro("SINGLE_CORE")
#define SINGLE_CORE 1
#include "src/utils/process.h"
#pragma pop_macro("SINGLE_CORE")

// paths
#define GEN_FLOAT_TABLES      "src/gen_float_tables.c"
#define GEN_FLOAT_TABLES_EXE  "gen_float_tables.exe"
#define GEN_FLOAT_TABLES_DEST "generated/float_tables.h"

#define LIB_CHARCONV     "src/test/alternatives/lib_charconv.cpp"
#define LIB_CHARCONV_DLL "generated/charconv.dll"

#define TEST_QFLOAT     "src/test/test_qfloat2.c"
#define TEST_QFLOAT_EXE "test_qfloat.exe"

void gen_float_tables();
void build_lib_charconv();
void run_tests();
void main_singlecore() {
  gen_float_tables();
  build_lib_charconv();
  run_tests();
}

void set_c99(BuildArgs *args) {
  arg_alloc(args, "-march=native");
  arg_alloc(args, "-masm=intel");
  arg_alloc(args, "-std=gnu99");
  arg_alloc(args, "-fno-signed-char");
  arg_alloc(args, "-Werror");
  arg_alloc(args, "-Wconversion");
  arg_alloc(args, "-Wsign-conversion");
  arg_alloc(args, "-Wnullable-to-nonnull-conversion");
}
void gen_float_tables() {
  BuildArgs args = {};
  arg_alloc(&args, GEN_FLOAT_TABLES);
  arg_alloc2(&args, "-o", GEN_FLOAT_TABLES_EXE);
  set_c99(&args);
  run_process("clang", &args);
  run_process("./" GEN_FLOAT_TABLES_EXE, 0);
}
void build_lib_charconv() {
  BuildArgs args = {};
  arg_alloc(&args, LIB_CHARCONV);
  arg_alloc2(&args, "-o", LIB_CHARCONV_DLL);
  arg_alloc(&args, "-shared");
  arg_alloc(&args, "-std=c++17");
  arg_alloc(&args, "-O2");
  run_process("clang", &args);
}
void run_tests() {
  BuildArgs args = {};
  // input
  arg_alloc(&args, TEST_QFLOAT);
  arg_alloc2(&args, "-o", TEST_QFLOAT_EXE);
  // c standard
  set_c99(&args);
  // linker
  arg_alloc(&args, "-fuse-ld=lld");
#if OS_WINDOWS
  arg_alloc(&args, "-Wl,/STACK:0x100000");
#elif OS_LINUX
  /* NOTE: linux forces it's own stack size on you, which you can only reduce at runtime */
#endif
  // params
#if OPT
  arg_alloc(&args, "-O2");
  arg_alloc(&args, "-flto");
  arg_alloc(&args, "-g");
#else
  arg_alloc(&args, "-O0");
  arg_alloc(&args, "-g");
#endif
#if NOLIBC
  arg_alloc(&args, "-nostdlib");
  arg_alloc(&args, "-fno-builtin");
  arg_alloc(&args, "-mno-stack-arg-probe");
  arg_alloc(&args, "-DNOLIBC");
#endif
#if SINGLE_CORE
  arg_alloc(&args, "-DSINGLE_CORE");
#endif
#if NDEBUG
  arg_alloc(&args, "-DNDEBUG");
#endif
  // compile
  // TODO: delete .pdb, .rdi?
  run_process("clang", &args);
  // run
  run_process("./" TEST_QFLOAT_EXE, 0);
}
