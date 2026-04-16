#pragma once
#include "builtin.h"
#include "fmt.h"
#include "mem.h"
#include "process.h"
#include "threads.h"

// entry
noreturn_ _init() {
#if NOLIBC && OS_WINDOWS
  asm volatile("" ::"X"(_fltused));
#endif
  _init_console();
  _init_page_fault_handler();
  _init_allocator(8 * MebiByte);
#if SINGLE_CORE
  thread_main(0);
#else
  _start_threads(_get_logical_core_count());
#endif
  exit_process(0);
}

#if NOLIBC
  /* NOTE: windows starts aligned to 8B, while linux starts (correctly) aligned to 16B
    thus we have to realign the stack pointer either way... */
  #if ARCH_X64
    #define _start_impl() asm volatile("xor ebp, ebp; and rsp, -16; call _init_process" ::: "rbp", "rsp");
  #else
ASSERT(false);
  #endif
naked noreturn_ _start() {
  _start_impl();
}
#else
CINT main() {
  _init();
}
#endif
