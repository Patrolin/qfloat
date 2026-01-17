#pragma once
#include "builtin.h"
#include "os.h"
#include "threads.h"

// TODO: do we care?, or do we just align to cache line?
#if ARCH_IS_64_BIT
ASSERT(__BIGGEST_ALIGNMENT__ == 16);
#elif ARCH_IS_32_BIT
ASSERT(__BIGGEST_ALIGNMENT__ == 8);
#else
ASSERT(false);
#endif

#if OS_WINDOWS
typedef enum : DWORD {
  EXCEPTION_ACCESS_VIOLATION = 0xC0000005,
} ExceptionCode;
  #define EXCEPTION_MAXIMUM_PARAMETERS 15
typedef struct {
  ExceptionCode ExceptionCode;
  DWORD ExceptionFlags;
  struct EXCEPTION_RECORD *ExceptionRecord;
  rawptr ExceptionAddress;
  DWORD NumberParameters;
  uintptr ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD;

OPAQUE(CONTEXT);
typedef struct {
  EXCEPTION_RECORD *ExceptionRecord;
  CONTEXT *ContextRecord;
} _EXCEPTION_POINTERS;
typedef enum : DWORD {
  EXCEPTION_EXECUTE_HANDLER = 1,
  EXCEPTION_CONTINUE_SEARCH = 0,
  EXCEPTION_CONTINUE_EXECUTION = -1,
} ExceptionResult;
typedef ExceptionResult ExceptionFilter(_EXCEPTION_POINTERS *exception);

typedef enum : DWORD {
  MEM_COMMIT = 1 << 12,
  MEM_RESERVE = 1 << 13,
  MEM_DECOMMIT = 1 << 14,
  MEM_RELEASE = 1 << 15,
} AllocTypeFlags;
typedef enum : DWORD {
  PAGE_READWRITE = 1 << 2,
} AllocProtectFlags;

// foreign ExceptionFilter* SetUnhandledExceptionFilter(ExceptionFilter filter_callback);
foreign Handle AddVectoredExceptionHandler(uintptr run_first, ExceptionFilter handler);
foreign intptr VirtualAlloc(intptr address, Size size, AllocTypeFlags type_flags, AllocProtectFlags protect_flags);
foreign BOOL VirtualFree(intptr address, Size size, AllocTypeFlags type_flags);
#elif OS_LINUX
typedef enum : u32 {
  PROT_EXEC = 1 << 0,
  PROT_READ = 1 << 1,
  PROT_WRITE = 1 << 2,
} ProtectionFlags;
typedef enum : u32 {
  MAP_PRIVATE = 1 << 1,
  MAP_ANONYMOUS = 1 << 5,
  MAP_GROWSDOWN = 1 << 8,
  MAP_STACK = 1 << 17,
} AllocTypeFlags;

intptr mmap(rawptr address, Size size, ProtectionFlags protection_flags, AllocTypeFlags type_flags, FileHandle fd, Size offset) {
  return syscall6(SYS_mmap, (uintptr)address, size, protection_flags, type_flags, (uintptr)fd, offset);
}
intptr munmap(intptr address, Size size) {
  return syscall2(SYS_munmap, (uintptr)address, size);
}
#endif

// page alloc
#if OS_WINDOWS
ExceptionResult _page_fault_handler(_EXCEPTION_POINTERS *exception_info) {
  EXCEPTION_RECORD *exception = exception_info->ExceptionRecord;
  DWORD exception_code = exception->ExceptionCode;
  if (expect_near(exception_code == EXCEPTION_ACCESS_VIOLATION)) {
    uintptr ptr = exception->ExceptionInformation[1];
    intptr page_ptr = intptr(ptr) & ~intptr(OS_PAGE_SIZE - 1);
    intptr commited_ptr = VirtualAlloc(page_ptr, OS_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
    return page_ptr != 0 && commited_ptr != 0 ? EXCEPTION_CONTINUE_EXECUTION : EXCEPTION_EXECUTE_HANDLER;
  }
  return EXCEPTION_EXECUTE_HANDLER;
}
void _init_page_fault_handler() {
  AddVectoredExceptionHandler(1, _page_fault_handler);
}
#else
  #define _init_page_fault_handler()
#endif

Bytes page_reserve(Size size) {
  Bytes buffer;
  buffer.size = size;
#if OS_WINDOWS
  buffer.ptr = (byte *)VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
  assert(buffer.ptr != 0);
#elif OS_LINUX
  buffer.ptr = (byte *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(intptr(buffer.ptr) != -1);
#else
  assert(false);
#endif
  return buffer;
}
void page_free(intptr ptr) {
#if OS_WINDOWS
  assert(VirtualFree(ptr, 0, MEM_RELEASE));
#elif OS_LINUX
  assert(munmap(ptr, 0) == 0);
#else
  assert(false);
#endif
}

// ring buffer
#define RING_BUFFER_SIZE 4096
ASSERT_POWER_OF_TWO(RING_BUFFER_SIZE);
typedef struct {
  u32 written;
  u64 value;
} RingBufferValue;
typedef struct {
  intptr buffer;
  i32 write_index;
  i32 default_read_index;
} RingBuffer;
void ring_buffer_write(RingBuffer *rb, u64 value) {
  // NOTE: wait-free population oblivious, but crash on overrun, zeroed by reader
  i32 write_index = atomic_fetch_add(&rb->write_index, sizeof(RingBufferValue));
  RingBufferValue *ptr = (RingBufferValue *)((rb->buffer + write_index) & (RING_BUFFER_SIZE - 1));
  assert2(ptr->written == 0, string("RingBuffer overrun"));
  ptr->value = value;
  atomic_store(&ptr->written, 1);
}
forward_declare void wait_on_address(u32 *address, u32 while_value);
void ring_buffer_write_or_wait(RingBuffer *rb, u64 value) {
  // NOTE: wait-free population oblivious, but wait on overrun, zeroed by reader
  i32 write_index = atomic_fetch_add(&rb->write_index, sizeof(RingBufferValue));
  RingBufferValue *ptr = (RingBufferValue *)((rb->buffer + write_index) & (RING_BUFFER_SIZE - 1));
  wait_on_address(&ptr->written, 1);
  ptr->value = value;
  atomic_store(&ptr->written, 1);
}
bool ring_buffer_read(RingBuffer *rb, u64 *value_ptr) {
  // NOTE: if there is only 1 reader, then wait-free population oblivious, else wait-free
  i32 *read_index_ptr = &rb->default_read_index;
  i32 read_index = atomic_load(read_index_ptr);
  while (1) {
    RingBufferValue *ptr = (RingBufferValue *)((rb->buffer + read_index) & (RING_BUFFER_SIZE - 1));
    if (ptr->written == 0) return false;
    if (atomic_compare_exchange(read_index_ptr, &read_index, read_index + i32(sizeof(RingBufferValue)))) {
      *value_ptr = ptr->value;
      atomic_store(&ptr->written, 0);
      return true;
    };
  }
}
bool ring_buffer_read_duplicated(RingBuffer *rb, u64 *value_ptr, i32 *read_index_ptr) {
  // NOTE: if each reader has its own `read_index`, then wait-free population oblivious, else wait-free
  i32 read_index = atomic_load(read_index_ptr);
  i32 write_index = rb->write_index;
  while (1) {
    RingBufferValue *ptr = (RingBufferValue *)((rb->buffer + read_index) & (RING_BUFFER_SIZE - 1));
    if (write_index - read_index <= 0 || ptr->written == 0) return false;
    if (atomic_compare_exchange(read_index_ptr, &read_index, read_index + i32(sizeof(RingBufferValue)))) {
      *value_ptr = ptr->value;
      atomic_store(&ptr->written, 0);
      return true;
    };
  }
}

// TODO: wait-free population oblivious general-purpose allocator instead of arena
/* NOTE: free_list_size(n): n < 8 ? n : 2**(floor(n/8) + 2) * (8 + n%8)/8 */
#define FREE_LIST_COUNT                  128
#define MEM_FLOAT_IMPLICIT_MANTISSA_BITS 3

#define MEM_FLOAT_MANTISSA_MAX (1 << MEM_FLOAT_IMPLICIT_MANTISSA_BITS)
typedef struct {
  intptr next;
} FreeList;
typedef struct {
  intptr next;
} FreeBlockHeader;
typedef struct {
  u16 size;
  u16 align_offset;
} UsedBlockHeader;
typedef struct {
  byte *buffer;
  Size buffer_size;
  FreeList free_lists[FREE_LIST_COUNT];
  intptr next;
  intptr end;
} Allocator;

uintptr _free_list_index_floor(Size size) {
  if (size < MEM_FLOAT_MANTISSA_MAX) return size;
  Size mantissa_start = index_first_one_floor(Size, size) - MEM_FLOAT_IMPLICIT_MANTISSA_BITS;
  Size exponent = (mantissa_start + 1) << MEM_FLOAT_IMPLICIT_MANTISSA_BITS;
  Size mantissa = (size >> mantissa_start) & (MEM_FLOAT_MANTISSA_MAX - 1);
  return exponent | mantissa;
}
intptr free_list_get(Allocator *allocator, Size size) {
  // TODO: this is too aggressive - do the proper algorithm
  uintptr free_list_index = _free_list_index_floor((size - 1) << 1);
  //..get item from first valid free_list
  return 0;
}
Size _free_list_size(uintptr n) {
  Size exponent = n >> MEM_FLOAT_IMPLICIT_MANTISSA_BITS;
  Size mantissa = n & (MEM_FLOAT_MANTISSA_MAX - 1);
  if (exponent == 0) return mantissa;
  return (mantissa | MEM_FLOAT_MANTISSA_MAX) << (exponent - 1);
}
intptr alloc(Allocator *allocator, Size size, Size align) {
  if (size == 0) return 0;
  //..if too big, just virtual alloc
  Size padded_size = max(sizeof(UsedBlockHeader) + (align - 1) + size, sizeof(FreeBlockHeader));
  intptr ptr = free_list_get(allocator, padded_size);
  if (ptr == 0) {
    ptr = atomic_fetch_add(&allocator->next, intptr(padded_size));
  }
  ptr = align_up(ptr, intptr(align));
  assert(ptr + intptr(size) <= allocator->end);
  memset((byte *)ptr, 0, size);
  return 0;
}
void free(Allocator *allocator, void *ptr) {
  //..push to global_threads->reclaim_queue
}
void reclaim_memory() {
  //..reclaim memory
}

// WFPO arena (not guaranteed to use minimal space)
typedef struct {
  intptr next;
  intptr end;
} ArenaAllocator;
global ArenaAllocator *global_arena;
ArenaAllocator *arena_allocator(Bytes buffer) {
  ArenaAllocator *arena = (ArenaAllocator *)buffer.ptr;
  arena->next = intptr(buffer.ptr + sizeof(ArenaAllocator));
  arena->end = intptr(buffer.ptr + buffer.size);
  return arena;
}

#define arena_alloc(arena, t)                      ((t *)arena_alloc_impl(arena, sizeof(t), alignof(t)))
#define arena_alloc_flexible(arena, t1, t2, count) ((t1 *)arena_alloc_impl(arena, sizeof(t1) + sizeof(t2) * count, alignof(t1)))
#define arena_alloc_array(arena, t, count)         ({          \
  ASSERT_MUlTIPLE_OF(sizeof(t), alignof(t));                   \
  (t *)arena_alloc_impl(arena, sizeof(t) * count, alignof(t)); \
})
intptr arena_alloc_impl(ArenaAllocator *arena, Size size, intptr align) {
  // assert(count_ones(align) == 1);
  intptr ptr = atomic_fetch_add(&arena->next, intptr(size) + align);
  ptr = align_up(ptr, align);
  assert(ptr + intptr(size) <= arena->end);
  memset((byte *)ptr, 0, size);
  return ptr;
}
static void arena_reset(ArenaAllocator *arena, intptr next) {
  arena->next = next;
  assert(atomic_load(&arena->next) == next); // assert single-threaded
}
