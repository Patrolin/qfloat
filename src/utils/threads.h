#pragma once
#include "builtin.h"
#include "os.h"

/* NOTE:
    BLOCKING - one thread can block many other threads (impacts throughput)
  STARVATION - one thread can be blocked indefinitely (impacts latency)
    LIVELOCK - threads can block each other indefinitely (impacts correctness)

  | NAME                           | BLOCKING | STARVATION | LIVELOCK | O(threads) | EXAMPLE                      |
  | blocking                       | yes      | yes        | no       | no         | wait_for_mutex(&lock)        |
  | starvation-free                | yes      | no         | no       | no         | wait_for_ticket_mutex(&lock) |
  | obstruction-free               | no       | yes        | yes      | no         | ?                            |
  | lock-free                      | no       | yes        | no       | no         | CAS loops                    |
  | wait-free                      | no       | no         | no       | yes        | helping                      |
  | wait-free population oblivious | no       | no         | no       | no         | atomics + UB                 |
*/

// syscalls
#if OS_WINDOWS
STRUCT(SYSTEM_INFO) {
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
};
typedef DWORD PTHREAD_START_ROUTINE(rawptr param);
typedef enum : DWORD {
  STACK_SIZE_PARAM_IS_A_RESERVATION = 0x00010000,
} CreateThreadFlags;
DISTINCT(Handle, ThreadHandle);

  #pragma comment(lib, "Synchronization.lib")
foreign void GetSystemInfo(SYSTEM_INFO *lpSystemInfo);
foreign ThreadHandle CreateThread(readonly SECURITY_ATTRIBUTES *security,
                                  usize stack_size,
                                  PTHREAD_START_ROUTINE start_proc,
                                  readonly rawptr param,
                                  DWORD flags,
                                  DWORD *thread_id);
foreign void WaitOnAddress(volatile rawptr address, readonly rawptr while_value, usize address_size, DWORD timeout);
foreign void WakeByAddressAll(readonly rawptr address);
#elif OS_LINUX
typedef CINT pid_t;
typedef u64 rlim_t;
typedef enum : CUINT {
  RLIMIT_STACK = 3,
} ResourceType;
STRUCT(rlimit) {
  rlim_t rlim_cur;
  rlim_t rlim_max;
};
isize getrlimit(ResourceType key, rlimit *value) {
  return syscall2(SYS_getrlimit, key, (uptr)value);
}
isize sched_getaffinity(pid_t pid, usize masks_size, u8 *masks) {
  return syscall3(SYS_sched_getaffinity, (uptr)pid, masks_size, (uptr)masks);
};

typedef enum : CUINT {
  CLONE_VM = 1 << 8,
  CLONE_FS = 1 << 9,
  CLONE_FILES = 1 << 10,
  CLONE_SIGHAND = 1 << 11,
  CLONE_PARENT = 1 << 15,
  CLONE_THREAD = 1 << 16,
  CLONE_SYSVSEM = 1 << 18,
  CLONE_IO = 1 << 31,
} ThreadFlags;
typedef enum : u64 {
  SIGABRT = 6,
  SIGKILL = 9,
  SIGCHLD = 17,
} SignalType;
// https://nullprogram.com/blog/2023/03/23/
typedef CUINT _linux_thread_entry(rawptr);
STRUCT_ALIGNED(new_thread_data, 16) {
  _linux_thread_entry *entry;
  rawptr param;
};
naked iptr newthread(ThreadFlags flags, new_thread_data *stack) {
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
typedef isize time_t;
STRUCT(timespec) {
  time_t t_sec;
  time_t t_nsec;
};
isize futex_wait(u32 *address, u32 while_value, readonly timespec *timeout) {
  return syscall4(SYS_futex, (uptr)address, FUTEX_WAIT, while_value, (uptr)timeout);
}
isize futex_wake(u32 *address, u32 count_to_wake) {
  return syscall3(SYS_futex, (uptr)address, FUTEX_WAKE, count_to_wake);
}
#else
// ASSERT(false);
#endif

// shared data
DISTINCT(u32, Thread);
#define Thread(x) ((Thread)(x))
STRUCT_ALIGNED(ThreadInfo, 32) {
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
};
ASSERT(sizeof(ThreadInfo) == 32);
ASSERT(alignof(ThreadInfo) == 32);
STRUCT(Threads) {
  ThreadInfo *thread_infos;
  u64 *values;
  u32 logical_core_count;
};
global Threads global_threads;

u32 _get_logical_core_count() {
#if RUN_SINGLE_THREADED
  return 1;
#else
  u32 logical_core_count;
  #if OS_WINDOWS
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  logical_core_count = info.dwNumberOfProcessors; /* NOTE: this fails above 64 cores... */
  #elif OS_LINUX
  u8 cpu_masks[64];
  isize written_masks_size = sched_getaffinity(0, sizeof(cpu_masks), (u8 *)&cpu_masks);
  assert(written_masks_size >= 0);
  for (isize i = 0; i < written_masks_size; i++) {
    logical_core_count += count_ones(u8, cpu_masks[i]);
  }
  #else
  assert(false);
  #endif
#endif
  assert(logical_core_count > 0);
  return logical_core_count;
}

// multi-core
void wait_on_address(u32 *address, u32 while_value) {
  /* NOTE: On Windows, WaitOnAddress() "is allowed to return for other reasons", same thing with futex() on Linux */
  while (atomic_load(address) == while_value) {
#if OS_WINDOWS
    WaitOnAddress(address, &while_value, sizeof(while_value), TIME_INFINITE);
#elif OS_LINUX
    futex_wait(address, while_value, 0);
#else
    assert(false);
#endif
  }
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
  u32 threads_start = global_threads.thread_infos[t].threads_start;
  u32 threads_end = global_threads.thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads.thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;

  u32 barrier = atomic_load(&shared_data->barrier);
  u32 barrier_stop = barrier + thread_count;
  u32 barrier_counter = atomic_add_fetch(&shared_data->barrier_counter, 1);
  if (expect_near(barrier_counter != barrier_stop)) {
    wait_on_address(&shared_data->barrier, barrier);
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
  u32 threads_start = global_threads.thread_infos[t].threads_start;
  ThreadInfo *shared_data = &global_threads.thread_infos[threads_start];

  bool is_first = atomic_fetch_add(&shared_data->is_first_counter, 1) == 0;
  if (expect_near(is_first)) {
    shared_data->was_first_thread = t;
  }
  return is_first;
}
/* scatter the value from the thread where single_core() returned true, defaulting to the first thread in the group */
#define barrier_scatter(t, value) barrier_scatter_impl(t, (u64 *)(value));
void barrier_scatter_impl(Thread t, u64 *value) {
  Thread threads_start = global_threads.thread_infos[t].threads_start;
  ThreadInfo *shared_data = &global_threads.thread_infos[threads_start];
  u64 *shared_value = &global_threads.values[threads_start];
  /* NOTE: we'd prefer if only the was_first_thread accessed shared_value here */
  if (expect_near(t == shared_data->was_first_thread)) {
    *shared_value = *value;
  }
  barrier(t); /* NOTE: make sure the scatter thread has written the data */
  *value = *shared_value;
  barrier(t); /* NOTE: make sure all threads have read the data */
}
/* gather values from all threads in the current group into a all threads */
#define barrier_gather(t, value) barrier_gather_impl(t, u64(value))
u64 *barrier_gather_impl(Thread t, u64 value) {
  global_threads.values[t] = value;
  barrier(t); /* NOTE: make sure all threads have written their data */
  return global_threads.values;
}

// split/join threads
bool barrier_split_threads(Thread t, u32 n) {
  // inline barrier() + modify threads
  u32 threads_start = global_threads.thread_infos[t].threads_start;
  u32 threads_end = global_threads.thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads.thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;
  Thread threads_split = threads_start + n;
  assert(n <= thread_count);

  u32 barrier = atomic_load(&shared_data->barrier);
  u32 barrier_stop = barrier + thread_count;
  u32 barrier_counter = atomic_add_fetch(&shared_data->barrier_counter, 1);
  if (expect_near(barrier_counter != barrier_stop)) {
    wait_on_address(&shared_data->barrier, barrier);
  } else {
    // modify threads
    for (Thread i = threads_start; i < threads_end; i++) {
      ThreadInfo *thread_data = &global_threads.thread_infos[i];
      /* NOTE: compiler unrolls this 4x */
      u32 *ptr = i < threads_split ? &thread_data->threads_end : &thread_data->threads_start;
      *ptr = threads_split;
    }
    ThreadInfo *split_data = &global_threads.thread_infos[threads_split];
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
  ThreadInfo *shared_data = &global_threads.thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;
  /* NOTE: `thread_count > prev_thread_count`, so we need a separate barrier */
  u32 barrier = atomic_load(&shared_data->join_barrier);
  u32 barrier_stop = barrier + thread_count;
  u32 barrier_counter = atomic_add_fetch(&shared_data->join_barrier_counter, 1);
  if (expect_near(barrier_counter != barrier_stop)) {
    wait_on_address(&shared_data->join_barrier, barrier);
  } else {
    // modify threads
    for (Thread i = threads_start; i < threads_end; i++) {
      ThreadInfo *thread_data = &global_threads.thread_infos[i];
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

// thread entry
forward_declare void thread_main(Thread t);
CUINT thread_entry(rawptr param) {
  Thread t = Thread(uptr(param));
  thread_main(t);
  barrier_join_threads(t, 0, global_threads.logical_core_count);
  return 0;
}
void _start_threads(u32 thread_count) {
  ThreadInfo thread_infos[thread_count] = {};
  u64 values[thread_count];
  global_threads.logical_core_count = thread_count;
  global_threads.thread_infos = thread_infos;
  global_threads.values = values;
  for (Thread t = 0; t < thread_count; t++) {
    global_threads.thread_infos[t].threads_end = thread_count;
    if (expect_near(t > 0)) {
#if OS_WINDOWS
      assert(CreateThread(0, 0, rawptr(thread_entry), (rawptr)uptr(t), STACK_SIZE_PARAM_IS_A_RESERVATION, 0) != 0);
#elif OS_LINUX
      rlimit stack_size_limit;
      assert(getrlimit(RLIMIT_STACK, &stack_size_limit) >= 0);
      u64 stack_size = stack_size_limit.rlim_cur;
      uptr stack = mmap(0, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK, -1, 0);
      assert(stack != -1);
  #if ARCH_STACK_DIRECTION == -1
      stack = stack + stack_size - sizeof(new_thread_data);
      new_thread_data *stack_data = (new_thread_data *)(stack);
  #else
      new_thread_data *stack_data = (new_thread_data *)(stack);
      stack = stack + sizeof(new_thread_data);
  #endif
      stack_data->entry = thread_wrapper;
      stack_data->param = (rawptr)uptr(t);
      ThreadFlags flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM;
      /* NOTE: SIGCHLD is the only one that doesn't print garbage depending on which thread exits... */
      isize error = newthread(flags | (ThreadFlags)SIGCHLD, stack_data);
      assert(error >= 0);
#else
      assert(false);
#endif
    }
  }
  thread_entry(0);
}
