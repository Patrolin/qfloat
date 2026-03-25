#pragma once
#include "builtin.h"
#include "math.h"
#include "mem.h"

// params
#define ARRAY_DEFAULT_SIZE            4
#define ARRAY_ALLOC(size, align_mask) alloc_size((size), (align_mask))
#define ARRAY_FREE(ptr)
#define MAP_HASH(x)         noise_u64(usize(x))
#define MAP_KEY_TYPE        isize
#define MAP_FILL_PERCENTAGE 75
/* TODO: use a gp allocator */
/* TODO: array_pop() */

// array
#define array(T)          T *
#define array_header(arr) ((ArrayHeader *)(uptr(*(arr)) - sizeof(ArrayHeader)))
STRUCT_PACKED(ArrayHeader) {
  u8 align_offset;
  isize capacity;
  isize count;
};

#define len(arr)                                          (*(arr) == nil ? 0 : array_header(arr)->count)
#define array_push(arr, value)                            (_array_grow((void **)(arr), sizeof(**(arr)), alignof(**(arr)) - 1), ((*(arr))[(array_header(arr))->count++] = value))
#define _array_size(item_size, item_align_mask, capacity) (item_align_mask + sizeof(ArrayHeader) + (item_size) * (capacity))
void _array_alloc(void **arr, usize item_size, usize item_align_mask, usize capacity, usize count) {
  usize required_size = _array_size(item_size, item_align_mask, capacity);
  uptr ptr = (uptr)ARRAY_ALLOC(required_size, 0);
  uptr data = align_up(ptr + sizeof(ArrayHeader), item_align_mask);
  u8 align_offset = (u8)align_up_offset(ptr + sizeof(ArrayHeader), item_align_mask);
  ArrayHeader *header = (ArrayHeader *)(data - sizeof(ArrayHeader));
  header->align_offset = align_offset;
  header->capacity = isize(capacity);
  header->count = isize(count);
  *(arr) = (rawptr)data;
}
void _array_grow(void **arr, usize item_size, usize item_align_mask) {
  void *old_data = *arr;
  if (old_data == nil) {
    _array_alloc(arr, item_size, item_align_mask, ARRAY_DEFAULT_SIZE, 0);
  } else {
    ArrayHeader *header = array_header(arr);
    usize count = usize(header->count);
    usize capacity = usize(header->capacity);
    if (expect_near(count < capacity)) return;
    _array_alloc(arr, item_size, item_align_mask, capacity * 2, count);
    memcpy(*arr, old_data, item_size * count);
    ARRAY_FREE(old_ptr);
  }
}
void array_free(void **arr) {
  if (*arr != nil) {
    ArrayHeader *header = array_header(arr);
    uptr ptr = uptr(header) - header->align_offset;
    ARRAY_FREE(ptr);
    *arr = nil;
  }
}

// map
#define map(V)                      V *
#define map_header(arr)             ((MapHeader *)(uptr(*(arr)) - sizeof(MapHeader)))
#define _map_slots(arr, value_size) ((MapSlot *)(align_up(uptr(*(arr)) + uptr(len(arr)) * value_size, alignof(MapSlot))))
STRUCT(MapHeader) {
  isize filled;
  isize capacity; /* NOTE: `isize count` for `len(map)` */
};
ASSERT(sizeof(MapHeader) >= __BIGGEST_ALIGNMENT__);
ASSERT(sizeof(MapHeader) % __BIGGEST_ALIGNMENT__ == 0);

STRUCT(MapSlot) {
  bool used;
  isize key;
};
#define map_slot(arr, i)           _map_slots((arr), sizeof(**(arr)))[i]
#define map_has(arr, key)          (_map_find((arr), (key), sizeof(**(arr))) >= 0)
#define map_get(arr, key, or_else) ({                      \
  isize _map_i = _map_find((arr), (key), sizeof(**(arr))); \
  _map_i >= 0 ? (*(arr))[_map_i] : (or_else);              \
});
#define map_set(arr, key, value)                                                            \
  do {                                                                                      \
    isize _map_i = _map_grow((void **)(arr), (key), sizeof(**(arr)), alignof(**(arr)) - 1); \
    (*(arr))[_map_i] = value;                                                               \
  } while (0);
#define map_remove(arr, key) TODO_tombstones
isize _map_find(void **arr, MAP_KEY_TYPE key, usize value_size) {
  if (*arr == nil) return -1;
  usize arr_len = usize(len(arr));
  MapSlot *slots = _map_slots(arr, value_size);
  usize i = MAP_HASH(key) % arr_len;
  usize probe = i | 1;
  while (1) {
    MapSlot slot = slots[i];
    if (!slot.used) return -isize(i);
    if (slot.key == key) return isize(i);
    i = (i + probe) % arr_len;
  }
}
isize _map_grow(void **arr, MAP_KEY_TYPE key, usize value_size, usize value_align_mask) {
  if (*arr == nil) {
    usize required_size = sizeof(MapHeader) + value_size * ARRAY_DEFAULT_SIZE + (alignof(MapSlot) - 1) + sizeof(MapSlot) * ARRAY_DEFAULT_SIZE;
    MapHeader *header = (MapHeader *)ARRAY_ALLOC(required_size, value_align_mask);
    header->filled = 0;
    header->capacity = ARRAY_DEFAULT_SIZE;
    *arr = header + 1;
  }
  isize i = _map_find(arr, key, value_size);
  if (i >= 0) return i;
  i = -i;
  MapHeader *header = map_header(arr);
  header->filled += 1;
  if (header->filled * 100 > header->capacity * MAP_FILL_PERCENTAGE) {
    assert(false); // TODO: move the map
  } else {
    MapSlot *slots = _map_slots(arr, value_size);
    MapSlot *slot = &slots[i];
    slot->used = true;
    slot->key = key;
    return i;
  }
}

// set
#define set(V)                 V *
#define set_has(arr, value)    (_map_find((arr), (value), sizeof(**(arr))) >= 0)
#define set_add(arr, value)    _map_grow((arr), (value), sizeof(**(arr)), alignof(**(arr)) - 1)
#define set_remove(arr, value) map_remove((arr), (value))
