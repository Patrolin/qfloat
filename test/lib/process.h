#pragma once
#include "definitions.h"
#include "os.h"
#include "mem.h"

// init
void init_console() {
#if OS_WINDOWS
  SetConsoleOutputCP(CP_UTF8);
#else
  // ASSERT(false);
#endif
}

forward_declare void init_page_fault_handler();
ArenaAllocator* global_arena;
void init_shared_arena() {
  Bytes buffer = page_reserve(VIRTUAL_MEMORY_TO_RESERVE);
  global_arena = arena_allocator(buffer);
}

forward_declare void start_threads();
forward_declare Noreturn exit_process(CINT exit_code);

// entry
Noreturn _start_process() {
#if OS_WINDOWS && !HAS_CRT
  asm volatile("" ::"m"(_fltused));
#endif
  init_console();
  init_page_fault_handler();
  init_shared_arena();
  start_threads();
  exit_process(0);
}
#if HAS_CRT
CINT main() {
  _start_process();
}
#else
  #if 0
/* NOTE: naked attribute for correctness, but we don't really need it,
  since we have to align the stack pointer manually either way...
  NOTE: this doesn't actually work together with -flto */
naked Noreturn _start() {
  ALIGN_STACK_POINTER();
  CALL(_start_process);
}
  #else
Noreturn _start() {
  ALIGN_STACK_POINTER();
  _start_process();
}
  #endif
#endif

// exit
Noreturn exit_process(CINT exit_code) {
#if OS_WINDOWS
  ExitProcess((CUINT)exit_code);
#elif OS_LINUX
  exit_group(exit_code);
#else
  ASSERT(false);
#endif
  for (;;);
}
Noreturn abort() {
  /* NOTE: technically you should signal abort on linux, but eh... */
  exit_process(1);
}
