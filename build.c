// clang build.c -o build.exe -march=native -masm=intel && ./build.exe
// clang build.c -o build.exe -march=native -masm=intel -fno-builtin -DNOLIBC && ./build.exe
#pragma push_macro("SINGLE_CORE")
#define SINGLE_CORE 1
#include "src/utils/entry.h"
#include "src/utils/process.h"
#pragma pop_macro("SINGLE_CORE")

// paths
#define FOO     "src/foo.c"
#define FOO_EXE "dist/foo.exe"

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
void run_foo();
void main_singlecore() {
  run_foo();
  // gen_float_tables();
  // build_lib_charconv();
  // run_tests();
}

void set_c99(string **args) {
  args_push(args, "-march=native");
  args_push(args, "-masm=intel");
  args_push(args, "-std=c99");
  args_push(args, "-fno-builtin"); /* NOTE: don't assume the types of functions like `free()` */
  args_push(args, "-fno-signed-char");
  args_push(args, "-Werror");
  args_push(args, "-Wconversion");
  args_push(args, "-Wsign-conversion");
  args_push(args, "-Wsign-compare");
  args_push(args, "-Wnullable-to-nonnull-conversion");
  args_push(args, "-Wuninitialized");
  args_push(args, "-Wconditional-uninitialized");
  args_push(args, "-Wswitch");
  args_push(args, "-Wimplicit-fallthrough");
#if NOLIBC
  args_push(args, "-nostdlib");
  args_push(args, "-mno-stack-arg-probe");
  args_push(args, "-DNOLIBC");
#endif
#if SINGLE_CORE
  args_push(args, "-DSINGLE_CORE");
#endif
#if DEBUG
  args_push(args, "-DDEBUG");
#endif
}
void run_foo() {
  string *args = nil;
  args_push(&args, FOO);
  args_push2(&args, "-o", FOO_EXE);
  set_c99(&args);
  run_process("clang", &args);
  run_process("./" FOO_EXE, 0);
}
void gen_float_tables() {
  string *args = nil;
  args_push(&args, GEN_FLOAT_TABLES);
  args_push2(&args, "-o", GEN_FLOAT_TABLES_EXE);
  set_c99(&args);
  run_process("clang", &args);
  run_process("./" GEN_FLOAT_TABLES_EXE, 0);
}
void build_lib_charconv() {
  string *args = nil;
  args_push(&args, LIB_CHARCONV);
  args_push2(&args, "-o", LIB_CHARCONV_DLL);
  args_push(&args, "-shared");
  args_push(&args, "-std=c++17");
  args_push(&args, "-O2");
  run_process("clang", &args);
}
void run_tests() {
  string *args = nil;
  // input
  args_push(&args, TEST_QFLOAT);
  args_push2(&args, "-o", TEST_QFLOAT_EXE);
  // c standard
  set_c99(&args);
  // linker
  args_push(&args, "-fuse-ld=lld");
#if OS_WINDOWS
  args_push(&args, "-Wl,/STACK:0x100000");
#elif OS_LINUX
  /* NOTE: linux forces it's own stack size on you, which you can only reduce at runtime */
#endif
  // params
#if OPT
  args_push(&args, "-O2");
  args_push(&args, "-flto");
  args_push(&args, "-g");
#else
  args_push(&args, "-O0");
  args_push(&args, "-g");
#endif
  // compile
  // TODO: delete .pdb, .rdi?
  run_process("clang", &args);
  // run
  run_process("./" TEST_QFLOAT_EXE, 0);
}
