#pragma once
#include "builtin.h"
#include "math.h"
#include "mem.h"

// params
#define DEFAULT_ARRAY_SIZE  4
#define hash(x)             noise_u64(usize(x))
#define MAP_KEY_TYPE        isize
#define MAP_FILL_PERCENTAGE 75

// array
#define array(T)          T *
#define array_header(arr) ((ArrayHeader *)(uptr(*(arr)) - sizeof(ArrayHeader)))
STRUCT(ArrayHeader) {
  isize capacity;
  isize count;
};
ASSERT(sizeof(ArrayHeader) >= __BIGGEST_ALIGNMENT__);
ASSERT(sizeof(ArrayHeader) % __BIGGEST_ALIGNMENT__ == 0);

#define len(arr)               (*(arr) == nil ? 0 : array_header(arr)->count)
#define array_push(arr, value) (_array_grow((void **)(arr), sizeof(**(arr)), alignof(**(arr))), ((*(arr))[(array_header(arr))->count++] = value))
void _array_grow(void **arr, usize item_size, usize item_align) {
  assert(item_align <= __BIGGEST_ALIGNMENT__);
  if (*arr == nil) {
    usize required_size = sizeof(ArrayHeader) + item_size * DEFAULT_ARRAY_SIZE;
    ArrayHeader *header = (ArrayHeader *)arena_alloc_size(&global_arena, required_size, item_align);
    header->capacity = DEFAULT_ARRAY_SIZE;
    header->count = 0;
    *arr = header + 1;
  } else {
    ArrayHeader *header = array_header(arr);
    // void *data = *arr;
    if (header->count >= header->capacity) {
      assert(false); // TODO: move the array
    }
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
#define map_has(arr, key)          (_map_find((arr), (key)) >= 0)
#define map_get(arr, key, or_else) ({         \
  isize _map_i = _map_find((arr), (key));     \
  _map_i >= 0 ? (*(arr))[_map_i] : (or_else); \
});
#define map_set(arr, key, value)                                                        \
  do {                                                                                  \
    isize _map_i = _map_grow((void **)(arr), (key), sizeof(**(arr)), alignof(**(arr))); \
    (*(arr))[_map_i] = value;                                                           \
  } while (0);
#define map_remove(arr, key) TODO_tombstones
isize _map_find(void **arr, MAP_KEY_TYPE key, usize value_size) {
  if (*arr == nil) return -1;
  usize arr_len = usize(len(arr));
  MapSlot *slots = _map_slots(arr, value_size);
  usize i = hash(key) % arr_len;
  usize probe = i | 1;
  while (1) {
    MapSlot slot = slots[i];
    if (!slot.used) return -isize(i);
    if (slot.key == key) return isize(i);
    i = (i + probe) % arr_len;
  }
}
isize _map_grow(void **arr, MAP_KEY_TYPE key, usize value_size, usize value_align) {
  if (*arr == nil) {
    usize required_size = sizeof(MapHeader) + value_size * DEFAULT_ARRAY_SIZE + (alignof(MapSlot) - 1) + sizeof(MapSlot) * DEFAULT_ARRAY_SIZE;
    MapHeader *header = (MapHeader *)arena_alloc_size(&global_arena, required_size, value_align);
    header->filled = 0;
    header->capacity = DEFAULT_ARRAY_SIZE;
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
#define set_has(arr, value)    (_map_find((arr), (value), sizeof(**(arr)), alignof(**(arr))) >= 0)
#define set_add(arr, value)    _map_grow((arr), (value), sizeof(**(arr)), alignof(**(arr)))
#define set_remove(arr, value) map_remove((arr), (value))
