#pragma once
#include "builtin.h"
#include "os.h"

/* NOTE:
                                 blocking - a suspended thread can starve all other threads indefinitely        (`wait_for_mutex(&lock)`)
                                          - throughput and latency degrade with number of threads
                          starvation-free - a suspended thread can only starve other threads for a finite time  (`wait_for_ticket_mutex(&lock)`)
                                          - throughput degrades with number of threads
                             non-blocking - a suspended thread does not impact other threads
                         obstruction-free - a thread can only make progress if no other threads are interfering (optimistic reads)
                                          - can livelock
                                lock-free - one thread can make progress, but others may starve                 (CAS loops)
                                          - latency degrades with number of threads
                                wait-free - all threads can make progress, but one may still starve             (helping + CAS loops)
                                          - latency degrades with number of threads
    wait-free population oblivious (WFPO) - all threads can make progress                                       (helping)
                                          - throughput and latency don't degrade with number of threads
  TODO: ~make a ringbuffer that's wait-free for SPSC, but lock-free for whichever side has multiple threads~
        make a wait-free ringbuffer (one thread can get stuck)
   - MPSC, ... via each thread stack allocates a ring buffer and `barrier_xx()`
  TODO: WFPO structures?
  TODO: wait-free alloc(allocator, size, align) {
    AllocOperation my_operation = alloc_operation(size, align);
    allocator->operations[t] = my_operation;
    while(1) {
      Thread in_flight = -1;
      atomic_compare_exchange(&allocator->in_flight, &in_flight, t);
      if (in_flight == -1) {in_flight = t}
      // ..help the inflight operation
      atomic_compare_exchange(&allocator->in_flight, in_flight, -1);
      if (in_flight == t) return;
    }
  }
*/

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
typedef alignto(16) struct {
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
typedef alignto(32) struct {
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
  ThreadInfo thread_infos[] flexible(logical_core_count);
} Threads;
ASSERT(sizeof(Threads) == 32);
ASSERT(alignof(Threads) == 32);
global Threads *global_threads;

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
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
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
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo *shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;

  bool is_first = atomic_fetch_add(&shared_data->is_first_counter, 1) == 0;
  if (expect_near(is_first)) {
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
  if (expect_near(t == shared_data->was_first_thread)) {
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
  if (expect_near(barrier_counter != barrier_stop)) {
    wait_on_address(&shared_data->barrier, barrier);
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
  if (expect_near(barrier_counter != barrier_stop)) {
    wait_on_address(&shared_data->join_barrier, barrier);
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
