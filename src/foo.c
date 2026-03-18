#include "utils/builtin.h"
#define SINGLE_CORE 1
#include "utils/entry.h"
#include "utils/fmt.h"
#include "utils/array.h"

void main_singlecore() {
  // array
  array(i32) arr = nil;
  printfln("len(&arr): %", isize, len(&arr));
  array_push(&arr, 1);
  array_push(&arr, 2);
  array_push(&arr, 3);
  printfln("len(&arr): %", isize, len(&arr));
  printfln("align_offset: %", usize, array_header(&arr)->align_offset);
  for (isize i = 0; i < len(&arr); i++) {
    printfln("arr[%]: %", isize, i, i32, arr[i]);
  }
  // map
  map(isize) m = nil;
  map_set(&m, 1, 10);
  map_set(&m, 2, 20);
  map_set(&m, 3, 30);
  map_set(&m, 3, 99);
  for (isize i = 0; i < len(&m); i++) {
    MapSlot slot = map_slot(&m, i);
    if (slot.used) {
      printfln("slots[%]: used: %, k: %, v: %", isize, i, bool, slot.used, isize, slot.key, isize, m[i]);
    }
  }
}
