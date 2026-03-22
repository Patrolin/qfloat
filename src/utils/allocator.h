#pragma once
#include "builtin.h"
#include "mem.h"

STRUCT(XorFreeListNode) {
  uptr next;
};
STRUCT2(Allocator, 16) {
  // TODO: byte *buffer_start // (also the reclaim_queue)
  byte *buffer_next;
  byte *buffer_end;
  XorFreeListNode *free_lists[16];
};
STRUCT(AllocatorBlockHeader) {
  AllocatorBlockHeader *prev_block;
  /* u63 next_block, u1 is_used */
  uptr next_block;
};
ASSERT(sizeof(AllocatorBlockHeader) % __BIGGEST_ALIGNMENT__ == 0);
STRUCT(AllocatorFreeBlock) {
  XorFreeListNode free_list;
};
ASSERT(sizeof(AllocatorBlockHeader) + sizeof(AllocatorFreeBlock) == 24);

Allocator global_allocator = {};
void init_allocator(usize buffer_size) {
  Bytes buffer = page_reserve(buffer_size);
  global_allocator.buffer_next = buffer.ptr;
  global_allocator.buffer_end = buffer.ptr + buffer_size;
}
rawptr alloc(usize size) {
  // TODO: get next valid block
  size = max(size, sizeof(AllocatorFreeBlock)) + sizeof(AllocatorBlockHeader);
  AllocatorBlockHeader *block = nil;
  // make new block if necessary
  if (block == nil) {
    block = (AllocatorBlockHeader *)atomic_fetch_add(&global_allocator.buffer_next, iptr(size));
    assert(uptr(block) + size < uptr(global_allocator.buffer_end));
  }
  // TODO: split block if possible
  // mark block as used
  block->next_block |= 1;
  return block + 1;
}
void free(void *ptr) {
  //..push to global_threads->reclaim_queue
}
void reclaim_memory() {
  //..reclaim memory
}
