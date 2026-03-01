#pragma once
#include "builtin.h"
#include "mem.h"

// params
#define DEFAULT_ARRAY_SIZE 4

// implementation
STRUCT(ArrayHeader) {
  isize count;
  isize capacity;
};
ASSERT(__BIGGEST_ALIGNMENT__ <= sizeof(ArrayHeader));
#define array(T)          T *
#define array_header(arr) ((ArrayHeader *)(rawptr(*(arr)) - sizeof(ArrayHeader)))
void array_grow(void **arr, usize item_size, usize item_align) {
  if (*arr == nil) {
    ArrayHeader *header = (ArrayHeader *)arena_alloc_size(&global_arena, item_size * DEFAULT_ARRAY_SIZE + sizeof(ArrayHeader), item_align);
    header->count = 0;
    header->capacity = DEFAULT_ARRAY_SIZE;
    *arr = header + 1;
  } else {
    ArrayHeader *header = array_header(arr);
    //void *data = *arr;
    if (header->count >= header->capacity) {
      assert(false); // TODO: move the array
    }
  }
}
#define array_push(arr, value) (array_grow((void **)(arr), sizeof(**(arr)), alignof(**(arr))), ((*(arr))[(array_header(arr))->count++] = value))
#define array_len(arr)         (*(arr) == nil ? 0 : array_header(arr)->count)
