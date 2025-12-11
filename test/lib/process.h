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
  /* NOTE: windows starts aligned to 8B, while linux starts (correctly) aligned to 16B
  thus we have to realign the stack pointer either way... */
  #if ARCH_X64
    #define _start_impl() asm volatile("xor ebp, ebp; and rsp, -16; call _start_process" ::: "rbp", "rsp");
  #else
ASSERT(false);
  #endif
naked Noreturn _start() {
  _start_impl();
}
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
