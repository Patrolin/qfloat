#pragma once
#include "definitions.h"
#include "mem.h"
#include "os.h"
#include "process.h"

// syscalls
#if OS_WINDOWS
typedef struct {
  union {
    DWORD dwOemId;
    struct {
      WORD wProcessorArchitecture;
      WORD wReserved;
    } DUMMYSTRUCTNAME;
  } DUMMYUNIONNAME;
  DWORD dwPageSize;
  rawptr lpMinimumApplicationAddress;
  rawptr lpMaximumApplicationAddress;
  DWORD *dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
} SYSTEM_INFO;
typedef DWORD PTHREAD_START_ROUTINE(rawptr param);
typedef enum : DWORD {
  STACK_SIZE_PARAM_IS_A_RESERVATION = 0x00010000,
} CreateThreadFlags;
DISTINCT(Handle, ThreadHandle);

  #pragma comment(lib, "Synchronization.lib")
foreign void GetSystemInfo(SYSTEM_INFO *lpSystemInfo);
foreign ThreadHandle CreateThread(readonly SECURITY_ATTRIBUTES *security,
                                  Size stack_size,
                                  PTHREAD_START_ROUTINE start_proc,
                                  readonly rawptr param,
                                  DWORD flags,
                                  DWORD *thread_id);
foreign void WaitOnAddress(volatile rawptr address, readonly rawptr while_value, Size address_size, DWORD timeout);
foreign void WakeByAddressAll(readonly rawptr address);
#elif OS_LINUX
typedef CINT pid_t;
typedef u64 rlim_t;
typedef enum : CUINT {
  RLIMIT_STACK = 3,
} ResourceType;
typedef struct {
  rlim_t rlim_cur;
  rlim_t rlim_max;
} rlimit;
intptr getrlimit(ResourceType key, rlimit *value) {
  return syscall2(SYS_getrlimit, key, (uintptr)value);
}
intptr sched_getaffinity(pid_t pid, Size masks_size, u8 *masks) {
  return syscall3(SYS_sched_getaffinity, (uintptr)pid, masks_size, (uintptr)masks);
};

typedef enum : CUINT {
  CLONE_VM = 1 << 8,
  CLONE_FS = 1 << 9,
  CLONE_FILES = 1 << 10,
  CLONE_SIGHAND = 1 << 11,
  CLONE_PARENT = 1 << 15,
  CLONE_THREAD = 1 < 16,
  CLONE_SYSVSEM = 1 << 18,
  CLONE_IO = 1 < 31,
} ThreadFlags;
typedef enum : u64 {
  SIGABRT = 6,
  SIGKILL = 9,
  SIGCHLD = 17,
} SignalType;
// https://nullprogram.com/blog/2023/03/23/
typedef CUINT _linux_thread_entry(rawptr);
typedef align(16) struct {
  _linux_thread_entry *entry;
  rawptr param;
} new_thread_data;
naked intptr newthread(ThreadFlags flags, new_thread_data *stack) {
  #if ARCH_X64
  asm volatile("mov eax, 56;" // rax = SYS_clone
               "syscall;"
               "mov rdi, [rsp+8];"
               "ret" ::: "rcx", "r11", "memory", "rax", "rdi", "rsi");
  #else
  assert(false);
  #endif
}

typedef enum : CINT {
  FUTEX_WAIT = 0,
  FUTEX_WAKE = 1,
} FutexOp;
typedef intptr time_t;
typedef struct {
  time_t t_sec;
  time_t t_nsec;
} timespec;
intptr futex_wait(u32 *address, u32 while_value, readonly timespec *timeout) {
  return syscall4(SYS_futex, (uintptr)address, FUTEX_WAIT, while_value, (uintptr)timeout);
}
intptr futex_wake(u32 *address, u32 count_to_wake) {
  return syscall3(SYS_futex, (uintptr)address, FUTEX_WAKE, count_to_wake);
}
#else
// ASSERT(false);
#endif

// shared data
DISTINCT(u32, Thread);
#define Thread(x) ((Thread)(x))
typedef align(32) struct {
  /* NOTE: barriers must be u32 on linux... */
  Thread threads_start;
  Thread threads_end;
  u32 is_first_counter;
  Thread was_first_thread;
  u32 barrier;
  u32 barrier_counter;
  /* NOTE: alternate between [counter, thread_count] and [thread_count, counter] */
  u32 join_barrier;
  u32 join_barrier_counter;
} ThreadInfo;
ASSERT(sizeof(ThreadInfo) == 32);
ASSERT(alignof(ThreadInfo) == 32);
typedef struct {
  u32 logical_core_count;
  u64 *values;
  ThreadInfo thread_infos[];
} Threads;
ASSERT(sizeof(Threads) == 32);
ASSERT(alignof(Threads) == 32);
global Threads *global_threads;

// entry
forward_declare void main_multicore(Thread t);
forward_declare void barrier_join_threads(Thread t, Thread threads_start, Thread threads_end);
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
    if (expect_likely(t > 0)) {
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

// multi-core
/* NOTE: wait on address, with a chance to wake up spuriously (including on windows...) */
void wait_on_address(u32 *address, u32 while_value) {
#if OS_WINDOWS
  WaitOnAddress(address, &while_value, sizeof(while_value), TIME_INFINITE);
#elif OS_LINUX
  futex_wait(address, while_value, 0);
#else
  assert(false);
#endif
}
void wake_all_on_address(u32 *address) {
#if OS_WINDOWS
  WakeByAddressAll(address);
#elif OS_LINUX
  futex_wake(address, MAX(i32));
#else
  assert(false);
#endif
}
/* wait until all threads enter this barrier() */
void barrier(Thread t) {
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;

  u32 barrier = atomic_load(&shared_data->barrier);
  u32 barrier_stop = barrier + thread_count;
  u32 barrier_counter = atomic_add_fetch(&shared_data->barrier_counter, 1);
  if (expect_likely(barrier_counter != barrier_stop)) {
    /* NOTE: On windows, WaitOnAddress() "is allowed to return for other reasons", same thing with futex() on linux */
    while (atomic_load(&shared_data->barrier) == barrier) {
      wait_on_address(&shared_data->barrier, barrier);
    }
  } else {
    /* NOTE: reset counters in case we have a non-power-of-two number of threads */
    shared_data->is_first_counter = 0;
    atomic_store(&shared_data->barrier, barrier_stop);
    wake_all_on_address(&shared_data->barrier);
  }
}

// single-core
/* return true on the first thread that gets here, and false on the rest */
bool single_core(Thread t) {
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;

  bool is_first = atomic_fetch_add(&shared_data->is_first_counter, 1) == 0;
  if (expect_small(is_first)) {
    shared_data->was_first_thread = t;
  }
  return is_first;
}
/* scatter the value from the thread where single_core() returned true, defaulting to the first thread in the group */
#define barrier_scatter(t, value) barrier_scatter_impl(t, (u64 *)(value));
void barrier_scatter_impl(Thread t, u64 *value) {
  Thread threads_start = global_threads->thread_infos[t].threads_start;
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
  u64 *shared_value = &global_threads->values[threads_start];
  /* NOTE: we'd prefer if only the was_first_thread accessed shared_value here */
  if (expect_unlikely(t == shared_data->was_first_thread)) {
    *shared_value = *value;
  }
  barrier(t); /* NOTE: make sure the scatter thread has written the data */
  *value = *shared_value;
  barrier(t); /* NOTE: make sure all threads have read the data */
}
/* gather values from all threads in the current group into a all threads */
#define barrier_gather(t, value) barrier_gather_impl(t, u64(value))
ThreadInfo *barrier_gather_impl(Thread t, u64 value) {
  global_threads->values[t] = value;
  barrier(t); /* NOTE: make sure all threads have written their data */
  return global_threads->thread_infos;
}

// split/join threads
bool barrier_split_threads(Thread t, u32 n) {
  // inline barrier() + modify threads
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;
  Thread threads_split = threads_start + n;
  assert(n <= thread_count);

  u32 barrier = atomic_load(&shared_data->barrier);
  u32 barrier_stop = barrier + thread_count;
  u32 barrier_counter = atomic_add_fetch(&shared_data->barrier_counter, 1);
  if (expect_likely(barrier_counter != barrier_stop)) {
    while (atomic_load(&shared_data->barrier) == barrier) {
      wait_on_address(&shared_data->barrier, barrier);
    }
  } else {
    // modify threads
    for (Thread i = threads_start; i < threads_end; i++) {
      ThreadInfo *thread_data = &global_threads->thread_infos[i];
      /* NOTE: compiler unrolls this 4x */
      u32 *ptr = i < threads_split ? &thread_data->threads_end : &thread_data->threads_start;
      *ptr = threads_split;
    }
    ThreadInfo *split_data = &global_threads->thread_infos[threads_split];
    split_data->was_first_thread = threads_split;
    /* NOTE: reset counters in case we have a non-power-of-two number of threads */
    shared_data->is_first_counter = 0;
    split_data->is_first_counter = 0;
    // -modify threads
    atomic_store(&split_data->barrier, barrier_stop);
    wake_all_on_address(&shared_data->barrier);
  }
  return t < threads_split;
}
void barrier_join_threads(Thread t, Thread threads_start, Thread threads_end) {
  // inline barrier() + modify threads
  assert(t >= threads_start && t < threads_end);
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;
  /* NOTE: `thread_count > prev_thread_count`, so we need a separate barrier */
  u32 barrier = atomic_load(&shared_data->join_barrier);
  u32 barrier_stop = barrier + thread_count;
  u32 barrier_counter = atomic_add_fetch(&shared_data->join_barrier_counter, 1);
  if (expect_likely(barrier_counter != barrier_stop)) {
    while (atomic_load(&shared_data->join_barrier) == barrier) {
      wait_on_address(&shared_data->join_barrier, barrier);
    }
  } else {
    // modify threads
    for (Thread i = threads_start; i < threads_end; i++) {
      ThreadInfo *thread_data = &global_threads->thread_infos[i];
      thread_data->threads_start = threads_start;
      thread_data->threads_end = threads_end;
    }
    shared_data->is_first_counter = 0;
    shared_data->was_first_thread = threads_start;
    // -modify threads
    atomic_store(&shared_data->join_barrier, barrier_stop);
    wake_all_on_address(&shared_data->join_barrier);
  }
}
