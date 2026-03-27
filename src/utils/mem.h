#pragma once
#include "builtin.h"
#include "os.h"

// params
#define RING_BUFFER_SIZE 4096
ASSERT_POWER_OF_TWO(RING_BUFFER_SIZE);

// page fault handler
#if OS_WINDOWS
typedef enum : DWORD {
  EXCEPTION_ACCESS_VIOLATION = 0xC0000005,
} ExceptionCode;
  #define EXCEPTION_MAXIMUM_PARAMETERS 15
STRUCT(EXCEPTION_RECORD) {
  ExceptionCode ExceptionCode;
  DWORD ExceptionFlags;
  struct EXCEPTION_RECORD *ExceptionRecord;
  rawptr ExceptionAddress;
  DWORD NumberParameters;
  uptr ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
};

OPAQUE(CONTEXT);
STRUCT(_EXCEPTION_POINTERS) {
  EXCEPTION_RECORD *ExceptionRecord;
  CONTEXT *ContextRecord;
};
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
foreign Handle AddVectoredExceptionHandler(uptr run_first, ExceptionFilter handler);
foreign uptr VirtualAlloc(uptr address, usize size, AllocTypeFlags type_flags, AllocProtectFlags protect_flags);
foreign BOOL VirtualFree(uptr address, usize size, AllocTypeFlags type_flags);
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

uptr mmap(rawptr address, usize size, ProtectionFlags protection_flags, AllocTypeFlags type_flags, FileHandle fd, usize offset) {
  return syscall6(SYS_mmap, (uptr)address, size, protection_flags, type_flags, (uptr)fd, offset);
}
uptr munmap(uptr address, usize size) {
  return syscall2(SYS_munmap, (uptr)address, size);
}
#endif

// page alloc
#if OS_WINDOWS
ExceptionResult _page_fault_handler(_EXCEPTION_POINTERS *exception_info) {
  EXCEPTION_RECORD *exception = exception_info->ExceptionRecord;
  DWORD exception_code = exception->ExceptionCode;
  if (expect_near(exception_code == EXCEPTION_ACCESS_VIOLATION)) {
    uptr ptr = exception->ExceptionInformation[1];
    uptr page_ptr = ptr & ~uptr(OS_PAGE_SIZE - 1);
    uptr commited_ptr = VirtualAlloc(page_ptr, OS_PAGE_SIZE, MEM_COMMIT, PAGE_READWRITE);
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

Bytes page_reserve(usize size) {
  Bytes buffer;
  buffer.size = size;
#if OS_WINDOWS
  buffer.ptr = (byte *)VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
  assert(buffer.ptr != 0);
#elif OS_LINUX
  buffer.ptr = (byte *)mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(iptr(buffer.ptr) != -1);
#else
  assert(false);
#endif
  return buffer;
}
void page_free(uptr ptr) {
#if OS_WINDOWS
  assert(VirtualFree(ptr, 0, MEM_RELEASE));
#elif OS_LINUX
  assert(munmap(ptr, 0) == 0);
#else
  assert(false);
#endif
}

// ring buffer
STRUCT(RingBuffer) {
  iptr buffer;
  i32 write_index;
  i32 default_read_index;
};
STRUCT(RingBufferValue) {
  u32 written;
  u64 value;
};
#define ring_buffer_size() (RING_BUFFER_SIZE * sizeof(RingBufferValue))
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
  // NOTE: if there is only 1 reader, then wait-free population oblivious, else lock-free
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
  // NOTE: if each reader has its own `read_index_ptr`, then wait-free population oblivious, else lock-free
  i32 read_index = atomic_load(read_index_ptr);
  i32 write_index = rb->write_index;
  assert2(write_index - read_index <= RING_BUFFER_SIZE, string("RingBuffer underrun"));
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

// multi-threaded O(1) allocator
STRUCT(XorFreeListNode) {
  uptr next;
};
STRUCT_ALIGNED(Allocator, 16) {
  // also the reclaim_queue
  byte *buffer_start;
  byte *buffer_next;
  byte *buffer_end;
  XorFreeListNode *free_lists[16];
};
enum PrevBlockSpecial : uptr {
  PREV_BLOCK_UNSET = 0,
  PREV_BLOCK_NONE = 1,
};
STRUCT(AllocatorBlockHeader) {
  // `PREV_BLOCK_UNSET | PREV_BLOCK_NONE | AllocatorBlockHeader*`
  uptr prev_block;
  // `u63 next_block, u1 is_mergable`
  uptr next_block;
};
ASSERT(sizeof(AllocatorBlockHeader) % __BIGGEST_ALIGNMENT__ == 0);
STRUCT(AllocatorFreeBlock) {
  XorFreeListNode free_list;
};
ASSERT(sizeof(AllocatorBlockHeader) + sizeof(AllocatorFreeBlock) == 24);

// TODO: per-thread free lists
Allocator global_allocator = {};
#define allocator_ring_buffer() ((RingBuffer *)(global_allocator.buffer_start + ring_buffer_size()))
void _init_allocator(usize buffer_size) {
  usize min_buffer_size = ring_buffer_size() + sizeof(RingBuffer);
  assert(buffer_size >= min_buffer_size);
  Bytes buffer = page_reserve(buffer_size);
  global_allocator.buffer_start = buffer.ptr;
  global_allocator.buffer_next = buffer.ptr + min_buffer_size;
  global_allocator.buffer_end = buffer.ptr + buffer_size;

  RingBuffer *rb = allocator_ring_buffer();
  rb->buffer = iptr(buffer.ptr);
  AllocatorBlockHeader *first_block = (AllocatorBlockHeader *)(global_allocator.buffer_next);
  first_block->prev_block = PREV_BLOCK_NONE;
}
// TODO: merge - if is_used atomic_compare_exchange(next_block, merged)
rawptr alloc_size(usize size, usize align_mask) {
  // TODO: get next valid block
  size = sizeof(AllocatorBlockHeader) + max(align_mask + size, sizeof(AllocatorFreeBlock));
  AllocatorBlockHeader *block = nil;
  // make new block if necessary
  if (block == nil) {
    block = (AllocatorBlockHeader *)atomic_fetch_add(&global_allocator.buffer_next, iptr(size));
    AllocatorBlockHeader *next_block = (AllocatorBlockHeader *)(uptr(block) + uptr(size));
    block->next_block = (uptr(next_block) << 1) | 1;
    atomic_store(&next_block->prev_block, uptr(block));
    assert(uptr(block) + size < uptr(global_allocator.buffer_end));
  } else {
    // TODO: split block if possible
  }
  // write block metadata
  block->next_block |= 1;
  uptr ptr = uptr(block + 1);
  if (align_mask != 0) {
    ptr = align_up(ptr, align_mask);
    usize align_offset = align_up_offset(ptr, align_mask);
    *(u8 *)(ptr - 1) = u8(align_offset);
  }
  return rawptr(ptr);
}
#define alloc_type(T)         ((T *)alloc_size(sizeof(T), alignof(T) - 1))
#define alloc_array(T, count) ((T *)alloc_size(sizeof(T) * count, alignof(T) - 1))

#define free_type(ptr, T)  free_size(sizeof(T), alignof(T) - 1)
#define free_array(ptr, T) free_size(ptr, alignof(T) - 1)
void free_size(void *ptr, usize align_mask) {
  RingBuffer *rb = allocator_ring_buffer();
  uptr block = uptr(ptr);
  if (align_mask != 0) {
    u8 align_offset = *(u8 *)(ptr - 1);
    block -= align_offset;
  }
  ring_buffer_write(rb, u64(block));
}
void reclaim_memory() {
  //..reclaim memory
}
