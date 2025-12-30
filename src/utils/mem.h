#pragma once
#include "definitions.h"
#include "os.h"

// page alloc
#if OS_WINDOWS
ExceptionResult _page_fault_handler(_EXCEPTION_POINTERS* exception_info) {
  EXCEPTION_RECORD* exception = exception_info->ExceptionRecord;
  DWORD exception_code = exception->ExceptionCode;
  if (expect_likely(exception_code == EXCEPTION_ACCESS_VIOLATION)) {
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
  buffer.ptr = (byte*)VirtualAlloc(0, size, MEM_RESERVE, PAGE_READWRITE);
  assert(buffer.ptr != 0);
#elif OS_LINUX
  buffer.ptr = (byte*)mmap(0, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
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

// arena
typedef struct {
  intptr next;
  intptr end;
} ArenaAllocator;
global ArenaAllocator* global_arena;
ArenaAllocator* arena_allocator(Bytes buffer) {
  ArenaAllocator* arena = (ArenaAllocator*)buffer.ptr;
  arena->next = intptr(buffer.ptr + sizeof(ArenaAllocator));
  arena->end = intptr(buffer.ptr + buffer.size);
  return arena;
}

#define arena_alloc(arena, t) ((t*)arena_alloc_impl(arena, sizeof(t), alignof(t) - 1))
#define arena_alloc_flexible(arena, t1, t2, count) ((t1*)arena_alloc_impl(arena, sizeof(t1) + sizeof(t2) * count, alignof(t1) - 1))
#define arena_alloc_array(arena, t, count) ({                     \
  ASSERT_MUlTIPLE_OF(sizeof(t), alignof(t));                      \
  (t*)arena_alloc_impl(arena, sizeof(t) * count, alignof(t) - 1); \
})
intptr arena_alloc_impl(ArenaAllocator* arena, Size size, intptr align_mask) {
  intptr current = atomic_load(&arena->next);
  intptr end = arena->end;
  while (true) {
    intptr ptr = (current + align_mask) & ~align_mask;
    intptr next = ptr + intptr(size);
    assert(next <= end);
    if (expect_exit(atomic_compare_exchange(&arena->next, &current, next))) {
      memset((byte*)ptr, 0, size);
      return ptr;
    };
  }
}
void arena_reset(ArenaAllocator* arena, intptr next) {
  /* NOTE: reset and assert single-threaded */
  intptr current = atomic_load(&arena->next);
  assert(atomic_compare_exchange(&arena->next, &current, next));
}
