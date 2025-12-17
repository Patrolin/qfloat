#pragma once
#include "definitions.h"
#include "os.h"
#include "mem.h"
#include "process.h"

// shared
DISTINCT(u32, Thread);
#define Thread(x) ((Thread)(x))
typedef align(32) struct {
  /* NOTE: barriers must be u32 on linux... */
  u32 barrier;
  u32 join_barrier;
  Thread threads_start;
  Thread threads_end;
  u32 is_first_counter;
  Thread was_first_thread;
  u32 is_last_counter;
  u32 join_counter;
} ThreadInfo;
ASSERT(sizeof(ThreadInfo) == 32);
ASSERT(alignof(ThreadInfo) == 32);
typedef struct {
  u32 logical_core_count;
  u64* values;
  ThreadInfo thread_infos[];
} Threads;
ASSERT(sizeof(Threads) == 32);
ASSERT(alignof(Threads) == 32);
Threads* global_threads;

// entry
forward_declare void main_multicore(Thread t);
forward_declare void barrier_join_threads(Thread t, Thread threads_start, Thread threads_end);
CUINT thread_entry(rawptr param) {
  Thread t = Thread(uintptr(param));
  main_multicore(t);
  barrier_join_threads(t, 0, global_threads->logical_core_count);
  return 0;
}

void start_threads() {
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
  intptr written_masks_size = sched_getaffinity(0, sizeof(cpu_masks), (u8*)&cpu_masks);
  assert(written_masks_size >= 0);
  for (intptr i = 0; i < written_masks_size; i++) {
    logical_core_count += count_ones(u8, cpu_masks[i]);
  }
  #else
  assert(false);
  #endif
#endif
  assert(logical_core_count > 0);
  global_threads = arena_alloc_flexible(global_arena, Threads, ThreadInfo, logical_core_count);
  u64* values = arena_alloc_count(global_arena, u64, logical_core_count);
  global_threads->logical_core_count = logical_core_count;
  global_threads->values = values;
  for (Thread t = 0; t < logical_core_count; t++) {
    global_threads->thread_infos[t].threads_end = logical_core_count;
    if (expect_likely(t > 0)) {
#if OS_WINDOWS
      assert(CreateThread(0, 0, thread_entry, rawptr(uintptr(t)), STACK_SIZE_PARAM_IS_A_RESERVATION, 0) != 0);
#elif OS_LINUX
      rlimit stack_size_limit;
      assert(getrlimit(RLIMIT_STACK, &stack_size_limit) >= 0);
      u64 stack_size = stack_size_limit.rlim_cur;
      intptr stack = mmap(0, stack_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_STACK, -1, 0);
      assert(stack != -1);
  #if ARCH_STACK_DIRECTION == -1
      stack = stack + intptr(stack_size) - intptr(sizeof(new_thread_data));
      new_thread_data* stack_data = (new_thread_data*)(stack);
  #else
      new_thread_data* stack_data = (new_thread_data*)(stack);
      stack = stack + sizeof(new_thread_data);
  #endif
      stack_data->entry = thread_entry;
      stack_data->param = rawptr(uintptr(t));
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
void wait_on_address(u32* address, u32 while_value) {
#if OS_WINDOWS
  WaitOnAddress(address, &while_value, sizeof(while_value), INFINITE);
#elif OS_LINUX
  do {
    futex_wait(address, while_value, 0);
  } while (volatile_load(address) == while_value);
#else
  assert(false);
#endif
}
void wake_all_on_address(u32* address) {
#if OS_WINDOWS
  WakeByAddressAll(address);
#elif OS_LINUX
  futex_wake(address, MAX(i32));
#else
  assert(false);
#endif
}
/* wait until all threads enter this barrier() and exit the previous barrier() */
void barrier(Thread t) {
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo* shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;

  u32 is_last_counter = atomic_add_fetch(&shared_data->is_last_counter, 1);
  if (expect_likely(is_last_counter < thread_count)) {
    wait_on_address(&shared_data->barrier, shared_data->barrier);
    atomic_fetch_add(&shared_data->is_last_counter, 1);
  } else {
    /* NOTE: reset counter in case we have a non-power-of-two number of threads */
    shared_data->is_first_counter = 0;
    /* NOTE: i'm not sure why, but this prevents a race condition by waiting until all threads have exited the previous barrier */
    shared_data->is_last_counter = u32(-thread_count + 1);
    shared_data->barrier += 1;
    wake_all_on_address(&shared_data->barrier);
  }
}

// single-core
/* return true on the first thread that gets here, and false on the rest */
bool single_core(Thread t) {
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo* shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;

  bool is_first = atomic_fetch_add(&shared_data->is_first_counter, 1) == 0;
  if (expect_small(is_first)) {
    shared_data->was_first_thread = t;
  }
  return is_first;
}
/* scatter the value from the thread where single_core() returned true, defaulting to the first thread in the group */
#define barrier_scatter(t, value) barrier_scatter_impl(t, (u64*)(value));
void barrier_scatter_impl(Thread t, u64* value) {
  Thread threads_start = global_threads->thread_infos[t].threads_start;
  ThreadInfo* shared_data = &global_threads->thread_infos[threads_start];
  u64* shared_value = &global_threads->values[threads_start];
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
ThreadInfo* barrier_gather_impl(Thread t, u64 value) {
  global_threads->values[t] = value;
  barrier(t); /* NOTE: make sure all threads have written their data */
  return global_threads->thread_infos;
}

// split/join threads
bool barrier_split_threads(Thread t, u32 n) {
  // inline barrier() + modify threads
  u32 threads_start = global_threads->thread_infos[t].threads_start;
  u32 threads_end = global_threads->thread_infos[t].threads_end;
  ThreadInfo* shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;
  Thread threads_split = threads_start + n;
  assert(n <= thread_count);

  bool is_last_counter = atomic_add_fetch(&shared_data->is_last_counter, 1);
  if (expect_likely(is_last_counter < thread_count)) {
    wait_on_address(&shared_data->barrier, shared_data->barrier);
    threads_start = global_threads->thread_infos[t].threads_start;
    shared_data = &global_threads->thread_infos[threads_start];
    atomic_fetch_add(&shared_data->is_last_counter, 1);
  } else {
    // modify threads
    for (Thread i = threads_start; i < threads_end; i++) {
      ThreadInfo* thread_data = &global_threads->thread_infos[i];
      /* NOTE: compiler unrolls this 4x */
      u32* ptr = i < threads_split ? &thread_data->threads_end : &thread_data->threads_start;
      *ptr = threads_split;
    }
    ThreadInfo* split_data = &global_threads->thread_infos[threads_split];
    split_data->was_first_thread = threads_split;
    /* NOTE: reset counters in case we have a non-power-of-two number of threads */
    shared_data->is_first_counter = 0;
    shared_data->is_last_counter = u32(-threads_split + 1);
    split_data->is_first_counter = 0;
    split_data->is_last_counter = u32(threads_split - thread_count + 1);
    // -modify threads
    shared_data->barrier += 1;
    wake_all_on_address(&shared_data->barrier);
  }
  return t < threads_split;
}
void barrier_join_threads(Thread t, Thread threads_start, Thread threads_end) {
  // inline barrier() + modify threads
  assert(t >= threads_start && t < threads_end);
  ThreadInfo* shared_data = &global_threads->thread_infos[threads_start];
  u32 thread_count = threads_end - threads_start;
  /* NOTE: `thread_count > prev_thread_count`, so we need a separate barrier */
  u32 join_counter = atomic_add_fetch(&shared_data->join_counter, 1);
  if (expect_likely(join_counter < thread_count)) {
    wait_on_address(&shared_data->join_barrier, shared_data->join_barrier);
    atomic_fetch_add(&shared_data->join_counter, 1);
  } else {
    // modify threads
    for (Thread i = threads_start; i < threads_end; i++) {
      ThreadInfo* thread_data = &global_threads->thread_infos[i];
      thread_data->threads_start = threads_start;
      thread_data->threads_end = threads_end;
    }
    shared_data->is_first_counter = 0;
    shared_data->was_first_thread = threads_start;
    shared_data->join_counter = u32(-thread_count + 1);
    shared_data->join_barrier += 1;
    // -modify threads
    wake_all_on_address(&shared_data->join_barrier);
  }
}
