#pragma once
#include <stdio.h>
#include "definitions.h"
#include "fmt.h"
#include "os.h"
#include "mem.h"

// init
void _init_console() {
#if OS_WINDOWS
  SetConsoleOutputCP(CP_UTF8);
#else
  // ASSERT(false);
#endif
}
forward_declare void _init_page_fault_handler();
ArenaAllocator* global_arena;
void _init_shared_arena() {
  Bytes buffer = page_reserve(VIRTUAL_MEMORY_TO_RESERVE);
  global_arena = arena_allocator(buffer);
}

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
  /* NOTE: technically you should signal abort on linux, or something? */
  exit_process(1);
}

// entry
#if SINGLE_CORE
forward_declare void main_singlecore();
#else
forward_declare void _init_threads();
#endif
Noreturn _init_process() {
#if OS_WINDOWS && NOLIBC
  asm volatile("" ::"m"(_fltused));
#endif
  _init_console();
  _init_page_fault_handler();
  _init_shared_arena();
#if SINGLE_CORE
  main_singlecore();
#else
  _init_threads();
#endif
  exit_process(0);
}
#if NOLIBC
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
#else
CINT main() {
  _init_process();
}
#endif

// build system
typedef struct {
#if OS_WINDOWS
  string* start;
#elif OS_LINUX
  rcstring* start;
#endif
  Size count;
} BuildArgs;
#define alloc_arg(args, arg) alloc_arg_impl(args, string(arg))
#define alloc_arg2(args, arg1, arg2)  \
  alloc_arg_impl(args, string(arg1)); \
  alloc_arg_impl(args, string(arg2))
static void alloc_arg_impl(BuildArgs* restrict args, string arg) {
#if OS_WINDOWS
  string* ptr = arena_alloc(global_arena, string);
#elif OS_LINUX
  rcstring* ptr = arena_alloc(global_arena, rcstring);
#endif
  *ptr = arg;
  args->start = args->start == 0 ? ptr : args->start;
  args->count += 1;
}
#define run_process(app, args) run_process_impl(string(app), args)
static void run_process_impl(readonly string app, readonly BuildArgs* args) {
#if OS_WINDOWS
  // copy to a single cstring
  byte* command = (byte*)global_arena->next;
  memcpy(command, app.ptr, app.size);
  byte* next = command + app.size;
  if (expect_likely(args != 0)) {
    for (intptr i = 0; i < args->count; i++) {
      string str = args->start[i];
      *(next++) = ' ';
      memcpy(next, str.ptr, str.size);
      next += str.size;
    }
  }
  (*next) = '\n';
  string command_str = (string){command, (Size)(next - command + 1)};
  print_string(command_str);
  (*next) = '\0';
  // start new process
  STARTUPINFOA startup_info = (STARTUPINFOA){
      .cb = sizeof(STARTUPINFOA),
  };
  PROCESS_INFORMATION process_info;
  bool ok = CreateProcessA(0, command, 0, 0, false, 0, 0, 0, &startup_info, &process_info);
  CloseHandle(process_info.hThread);
  assert(ok);
  WaitForSingleObject(process_info.hProcess, INFINITE);
  CloseHandle(process_info.hProcess);
#else
  // TODO: In a new thread: `execve(command, args, 0);`
  ASSERT(false);
#endif
  // assert single-threaded
  atomic_compare_exchange(&global_arena->next, (intptr*)&command, (intptr)command);
}
