#define SINGLE_CORE 1
#include "utils/entry.h"
#include "utils/fmt.h"
#include "utils/array.h"

void main_singlecore() {
  array(i32) arr = nil;
  printfln("l: %", isize, array_len(&arr));
  array_push(&arr, 1);
  array_push(&arr, 2);
  array_push(&arr, 3);
  printfln("l: %", isize, array_len(&arr));
  for (isize i = 0; i < array_len(&arr); i++) {
    printfln("i: %, v: %", isize, i, i32, arr[i]);
  }
}
