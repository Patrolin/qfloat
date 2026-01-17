#pragma once
#include "builtin.h"
#include "os.h"
#include "mem.h"
#include "process.h"
#include "threads.h"

// entry
#if OS_WINDOWS
typedef enum : DWORD {
  CP_UTF8 = 65001,
} CodePage;

foreign BOOL WINAPI SetConsoleOutputCP(CodePage code_page);
#elif OS_LINUX
#else
ASSERT(false);
#endif

void _init_console() {
#if OS_WINDOWS
  SetConsoleOutputCP(CP_UTF8);
#else
  // ASSERT(false);
#endif
}
void _init_shared_arena() {
  Bytes buffer = page_reserve(VIRTUAL_MEMORY_TO_RESERVE);
  global_arena = arena_allocator(buffer);
}
#if SINGLE_CORE
forward_declare void main_singlecore();
#else
forward_declare void main_multicore(Thread t);
CUINT thread_entry(rawptr param) {
  Thread t = Thread(uintptr(param));
  main_multicore(t);
  barrier_join_threads(t, 0, global_threads->logical_core_count);
  return 0;
}
void _init_threads() {
  // get `logical_core_count`
  u32 logical_core_count;
  #if RUN_SINGLE_THREADED
  logical_core_count = 1;
  #else
    #if OS_WINDOWS
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  logical_core_count = info.dwNumberOfProcessors; /* NOTE: this fails above 64 cores... */
    #elif OS_LINUX
  u8 cpu_masks[64];
  intptr written_masks_size = sched_getaffinity(0, sizeof(cpu_masks), (u8 *)&cpu_masks);
  assert(written_masks_size >= 0);
  for (intptr i = 0; i < written_masks_size; i++) {
    logical_core_count += count_ones(u8, cpu_masks[i]);
  }
    #else
  assert(false);
    #endif
  #endif
  assert(logical_core_count > 0);
  // start threads
  global_threads = arena_alloc_flexible(global_arena, Threads, ThreadInfo, logical_core_count);
  assert(global_threads != 0);
  u64 *values = arena_alloc_array(global_arena, u64, logical_core_count);
  global_threads->logical_core_count = logical_core_count;
  global_threads->values = values;
  for (Thread t = 0; t < logical_core_count; t++) {
    global_threads->thread_infos[t].threads_end = logical_core_count;
    if (expect_near(t > 0)) {
  #if OS_WINDOWS
      assert(CreateThread(0, 0, thread_entry, (rawptr)uintptr(t), STACK_SIZE_PARAM_IS_A_RESERVATION, 0) != 0);
  #elif OS_LINUX
      rlimit stack_size_limit;
      assert(getrlimit(RLIMIT_STACK, &stack_size_limit) >= 0);
      u64 stack_size = stack_size_limit.rlim_cur;
      intptr stack = mmap(0, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK, -1, 0);
      assert(stack != -1);
    #if ARCH_STACK_DIRECTION == -1
      stack = stack + intptr(stack_size) - intptr(sizeof(new_thread_data));
      new_thread_data *stack_data = (new_thread_data *)(stack);
    #else
      new_thread_data *stack_data = (new_thread_data *)(stack);
      stack = stack + sizeof(new_thread_data);
    #endif
      stack_data->entry = thread_entry;
      stack_data->param = (rawptr)uintptr(t);
      ThreadFlags flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM;
      /* NOTE: SIGCHLD is the only one that doesn't print garbage depending on which thread exits... */
      intptr error = newthread(flags | (ThreadFlags)SIGCHLD, stack_data);
      assert(error >= 0);
  #else
      assert(false);
  #endif
    }
  }
  thread_entry(0);
}
#endif

noreturn_ _init_process() {
#if OS_WINDOWS && NOLIBC
  asm volatile("" ::"X"(_fltused));
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
    #define _start_impl() asm volatile("xor ebp, ebp; and rsp, -16; call _init_process" ::: "rbp", "rsp");
  #else
ASSERT(false);
  #endif
naked noreturn_ _start() {
  _start_impl();
}
#else
CINT main() {
  _init_process();
}
#endif
