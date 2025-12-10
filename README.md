## Usage
```c
//#define QFLOAT_NOLIBC 1
//#define qfloat_copy(ptr, size, dest) copy(ptr, size, dest)
//#define qfloat_assert(condition) assert(condition)
/* NOTE: overwrites `#pragma STDC FENV_ACCESS` to `DEFAULT`, to disable float optimizations locally */
#include "qfloat.h"

int main() {
  char buffer[QFLOAT_SIZE_f64];
  // if using libc
  sprint_f64_libc(0.3, buffer);
  printf("%s", buffer);  // 0.3
  // else
  sprint_f64(0.2 + 0.1, buffer);
  printf("%s", buffer);  // 0.30000000000000004
}
```

## Motivation
We want to convert a float into the shortest necessary string representation, meaning:
1) We are able to convert the string back into the original float.
2) It is the shortest string representation of that float.

Consider the following floats:
```c
// 0x3333333333333333 (0.29999999999999999)
f64 x = 0.3;
// 0x3333333333333334 (0.30000000000000004)
f64 y = 0.2 + 0.1;
assert(y != x);
```

If we try to print these naively with libc, it will print up to 6 digits of precision (failing our first condition):
```c
printf("%g\n", x);  // "0.3"
printf("%g\n", y);  // "0.3"
```

If we try to force it to print more digits, it will fail our second condition:
```c
printf("%.17g\n", x);  // "0.29999999999999999"
printf("%.17g\n", y);  // "0.30000000000000004"
```
Since `"0.29999999999999999"` could be printed as just `"0.3"`.

We want something like:
```c
print_float(x);  // "0.3"
print_float(y);  // "0.30000000000000004"
```

## Existing solutions
There exist [algorithms](https://github.com/abolz/Drachennest) to find the shortest string representation of a float (for any float rounding mode), but they involve a lot of LoC and code-gen'd tables.

## Why printing floats is hard
1) Floats cannot represent most powers of 10 exactly, e.g. f64s can represent up to `1e22`.
2) We can represent the significand of an f64 in a u64/i64. However, \
to represent the value of an f64 in base 10, we need [17 digits](https://www.exploringbinary.com/number-of-digits-required-for-round-trip-conversions).
but the maximum safe integer (`x-1 != x`) is `2**53` can only represent `Math.log10(2**53) = ~15.9` digits,
so we lose precision when converting to an f64, e.g `(f64)16176163832269603ULL == 16176163832269604.0`.

## New approach
Instead of trying to find the shortest representation from scratch, why not start with the shortest sufficient representation (which is guaranteed to make our first condition true), and then shorten it until we get the shortest necessary representation:
```c
string sx = sprint_f64(x);  // "0.29999999999999999"
shorten_f64_string(sx);     // "0.3"

string sy = sprint_f64(y);  // "0.30000000000000004"
shorten_f64_string(sy);     // "0.30000000000000004"
```

We define a new operation `truncate_significand_upwards()`, which truncates the significand by one digit and then adds one to the last digit (with carry):
```c
truncate_significand_upwards("0.29999999999999999e16") -> "0.3000000000000000e16"
```
Then we can implement `shorten_f64_string()` as:
```c
string shorten_f64_string(string s) {
  // ...Ignore leading `'+'`, `'-'`
  // ...Ignore `"nan"`, `"inf"`, `"infinity"`
  while truncate_significand_upwards_is_valid_operation(s) {
    s = truncate_significand_upwards(s)
  }
  while truncate_significand_is_valid_operation(s) {
    s = truncate_significand(s)
  }
}
```

We can also define our own `sprint_f64()` instead of relying on the libc version, with IEEE augmented float operations
(which nobody implements, but are easy enough to define ourselves). This does give incorrect values when `abs(x) < ~1e-304`
, as we store the error in a float, which will eventually underflow to 0. However, such small values have little to no practical use anyway. \
Similarly we can convert i64 to f64 by also computing the error of the conversion.
