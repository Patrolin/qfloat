#pragma once
#include "builtin.h"

// TODO: delete this
/* NOTE: free_list_size(n): n < MEM_MANTISSA_MAX ? n : 2**(floor(n/MEM_MANTISSA_MAX) + MEM_IMPLICIT_MANTISSA_BITS-1) * (1 + (n%MEM_MANTISSA_MAX)/MEM_MANTISSA_MAX) */
#define MEM_IMPLICIT_MANTISSA_BITS 3

#define MEM_MANTISSA_MAX (1 << MEM_IMPLICIT_MANTISSA_BITS)
typedef u32 AllocatorBlockSize;
#define FREE_LIST_COUNT ((sizeof_bits(AllocatorBlockSize) - MEM_IMPLICIT_MANTISSA_BITS) * MEM_MANTISSA_MAX)
ASSERT(FREE_LIST_COUNT == 232);
/* TODO: remove this
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

/*
STRUCT(FreeList) {
  FreeList *free_list;
};
typedef AllocatorBlockSize AllocatorBlockCommon;
typedef u16 UsedBlockPrefix; // TODO: does any CPU have `CACHE_LINE_SIZE >= 256`?
STRUCT(FreeBlockHeader) {
  AllocatorBlockCommon common;
  AllocatorBlockSize distance_to_prev_block;
  FreeList *free_list;
};
STRUCT(GeneralAllocator) {
  byte *buffer;
  usize buffer_size;
  uptr next;
  uptr end;
};
STRUCT(AllocatorThreadData) {
  uptr free_lists[FREE_LIST_COUNT];
};
global GeneralAllocator global_allocator;

uptr _free_list_index_floor(usize size) {
  if (size < MEM_MANTISSA_MAX) return size;
  usize mantissa_start = index_first_one_floor(usize, size) - MEM_IMPLICIT_MANTISSA_BITS;
  usize exponent = (mantissa_start + 1) << MEM_IMPLICIT_MANTISSA_BITS;
  usize mantissa = (size >> mantissa_start) & (MEM_MANTISSA_MAX - 1);
  return exponent | mantissa;
}
uptr _free_list_get(GeneralAllocator *allocator, usize size) {
  // TODO: this is too aggressive - do the proper algorithm
  // uptr free_list_index = _free_list_index_floor((size - 1) << 1);
  //..get item from first valid free_list
  return 0;
}
usize _free_list_size(uptr n) {
  usize exponent = n >> MEM_IMPLICIT_MANTISSA_BITS;
  usize mantissa = n & (MEM_MANTISSA_MAX - 1);
  if (exponent == 0) return mantissa;
  return (mantissa | MEM_MANTISSA_MAX) << (exponent - 1);
}

#define alloc_type(t)                 ((t *)alloc_size(allocator, sizeof(t), alignof(t)))
#define alloc_array(t, count)         ((t *)alloc_size(allocator, sizeof(t) * count, alignof(t)))
#define alloc_flexible(t1, t2, count) ((t1 *)alloc_size(allocator, sizeof(t1) + sizeof(t2) * count, alignof(t1)))
#define alloc_size(size, align)       _alloc_impl(&global_allocator, size, max(align, alignof(UsedBlockPrefix)) - 1)
uptr _alloc_impl(GeneralAllocator *allocator, usize size, usize align_low_mask) {
  if (expect_far(size == 0)) return 0;
  //..if too big, just virtual alloc
  uptr padded_size = max(sizeof(AllocatorBlockCommon) + sizeof(UsedBlockPrefix) + align_low_mask + size, sizeof(FreeBlockHeader));
  uptr ptr = _free_list_get(allocator, padded_size);
  if (ptr == 0) {
    padded_size = align_up(padded_size, alignof(FreeBlockHeader)-1);
    ptr = atomic_fetch_add(&allocator->next, padded_size);
    assert(ptr + size <= allocator->end);
  }
  ptr = align_up(ptr, align_low_mask);
  uptr align_offset = align_offset(ptr, align_low_mask);
  *(UsedBlockPrefix *)(ptr - sizeof(UsedBlockPrefix)) = (UsedBlockPrefix)align_offset;
  memset((byte *)ptr, 0, size);
  return 0;
}
void free(GeneralAllocator *allocator, void *ptr) {
  //..push to global_threads->reclaim_queue
}
void reclaim_memory() {
  //..reclaim memory
}
*/
