// clang build.c -o build.exe && build.exe
#pragma push_macro("SINGLE_CORE")
#define SINGLE_CORE 1
#include "src/test/lib/process.h"
#pragma pop_macro("SINGLE_CORE")

#define INPUT_FILE "src/test/test_qfloat2.c"
#define OUTPUT_FILE "test_qfloat.exe"
void main_singlecore() {
  BuildArgs args = {};
  // input
  alloc_arg(&args, INPUT_FILE);
  alloc_arg(&args, "-o=" OUTPUT_FILE);
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
#if NDEBUG
  alloc_arg(&args, "-DNDEBUG");
#endif
  // compile
  // TODO: delete .pdb, .rdi?
  run_process("clang.exe", &args);
  // run
  run_process(OUTPUT_FILE, 0);
}
