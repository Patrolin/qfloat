#pragma once
#include "builtin.h"

// ring buffer
#define RING_BUFFER_SIZE 4096
ASSERT_POWER_OF_TWO(RING_BUFFER_SIZE);

STRUCT(RingBuffer) {
  intptr buffer;
  i32 write_index;
  i32 default_read_index;
};
STRUCT(RingBufferValue) {
  u32 written;
  u64 value;
};
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
/* NOTE: free_list_size(n): n < MEM_MANTISSA_MAX ? n : 2**(floor(n/MEM_MANTISSA_MAX) + MEM_IMPLICIT_MANTISSA_BITS-1) * (1 + (n%MEM_MANTISSA_MAX)/MEM_MANTISSA_MAX) */
#define MEM_IMPLICIT_MANTISSA_BITS 3

#define MEM_MANTISSA_MAX (1 << MEM_IMPLICIT_MANTISSA_BITS)
typedef u32 AllocatorBlockSize;
#define FREE_LIST_COUNT ((sizeof_bits(AllocatorBlockSize) - MEM_IMPLICIT_MANTISSA_BITS) * MEM_MANTISSA_MAX)
ASSERT(FREE_LIST_COUNT == 232);
/*
  AllocatorBlockCommon: u32 common // {u1 used, u31 size}
  UsedBlock: AllocatorBlockCommon common .. u16 align_offset, u8 data[]
  FreeBlock: AllocatorBlockCommon common, u32 distance_to_prev_block, FreeList* free_list ..

  bool mark_block_as_used :: proc(&block) {
    // TODO: return true if we were the one to change it
    return atomic_or(&block.common, 1 << 31)
  }
  bool mark_block_as_free :: proc(&block) {
    // TODO: return true if we were the one to change it
    return atomic_and(&block.common, ~(1 << 31))
  }
  bool merge_with_prev_block :: proc(&block) {
    prev_block := get_prev_block(block)
    prev_block_size := get_block_size(prev_block)
    block_size := get_block_size(block)
    return atomic_compare_exchange(&prev_block.common, prev_block_size, prev_block_size + block_size)
  }
*/
STRUCT(FreeList) {
  FreeList *free_list;
};
typedef AllocatorBlockSize AllocatorBlockCommon;
typedef u16 UsedBlockPrefix; /* TODO: does any CPU have `CACHE_LINE_SIZE >= 256`? */
STRUCT(FreeBlockHeader) {
  AllocatorBlockCommon common;
  AllocatorBlockSize distance_to_prev_block;
  FreeList *free_list;
};
STRUCT(GeneralAllocator) {
  byte *buffer;
  Size buffer_size;
  intptr next;
  intptr end;
};
STRUCT(AllocatorThreadData) {
  intptr free_lists[FREE_LIST_COUNT];
};
global GeneralAllocator global_allocator;

uintptr _free_list_index_floor(Size size) {
  if (size < MEM_MANTISSA_MAX) return size;
  Size mantissa_start = index_first_one_floor(Size, size) - MEM_IMPLICIT_MANTISSA_BITS;
  Size exponent = (mantissa_start + 1) << MEM_IMPLICIT_MANTISSA_BITS;
  Size mantissa = (size >> mantissa_start) & (MEM_MANTISSA_MAX - 1);
  return exponent | mantissa;
}
intptr _free_list_get(GeneralAllocator *allocator, Size size) {
  // TODO: this is too aggressive - do the proper algorithm
  uintptr free_list_index = _free_list_index_floor((size - 1) << 1);
  //..get item from first valid free_list
  return 0;
}
Size _free_list_size(uintptr n) {
  Size exponent = n >> MEM_IMPLICIT_MANTISSA_BITS;
  Size mantissa = n & (MEM_MANTISSA_MAX - 1);
  if (exponent == 0) return mantissa;
  return (mantissa | MEM_MANTISSA_MAX) << (exponent - 1);
}

#define alloc(t)                       ((t *)_alloc_preprocess(allocator, sizeof(t), alignof(t)))
#define alloc_array(t, count)          ((t *)_alloc_preprocess(allocator, sizeof(t) * count, alignof(t)))
#define alloc_flexible(t1, t2, count)  ((t1 *)_alloc_preprocess(allocator, sizeof(t1) + sizeof(t2) * count, alignof(t1)))
#define _alloc_preprocess(size, align) _alloc_impl(&global_allocator, size, max(align, alignof(UsedBlockPrefix)) - 1)
intptr _alloc_impl(GeneralAllocator *allocator, Size size, Size align_mask) {
  if (expect_far(size == 0)) return 0;
  //..if too big, just virtual alloc
  Size padded_size = max(sizeof(AllocatorBlockCommon) + sizeof(UsedBlockPrefix) + align_mask + size, sizeof(FreeBlockHeader));
  intptr ptr = _free_list_get(allocator, padded_size);
  if (ptr == 0) {
    padded_size = (Size)align_up(intptr(padded_size), alignof(FreeBlockHeader));
    ptr = atomic_fetch_add(&allocator->next, intptr(padded_size));
    assert(ptr + intptr(size) <= allocator->end);
  }
  intptr align_offset = intptr(align_mask + 1) - (ptr & intptr(align_mask));
  ptr = align_up(ptr, intptr(align_mask + 1));
  *(UsedBlockPrefix *)(ptr - intptr(sizeof(UsedBlockPrefix))) = (UsedBlockPrefix)align_offset;
  memset((byte *)ptr, 0, size);
  return 0;
}
void free(GeneralAllocator *allocator, void *ptr) {
  //..push to global_threads->reclaim_queue
}
void reclaim_memory() {
  //..reclaim memory
}
