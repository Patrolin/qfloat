#pragma once
#include <stdint.h>
#include <stdbool.h>

// builtins
#ifndef qf_assert
  #define qf_assert(condition) assert(condition)
  #ifndef assert
    #include "assert.h"
  #endif
#endif
#ifndef QF_ASSERT
  #if defined(__cplusplus) || (defined(_MSC_VER) && !defined(__clang__))
    #define QF_ASSERT(condition) static_assert(condition, #condition)
  #else
    #define QF_ASSERT(condition) _Static_assert(condition, #condition)
  #endif
#endif
#ifndef qf_nonnull
  #if defined(__clang__) || defined(__GNUC__)
    #define qf_nonnull(...)    __attribute__((nonnull(__VA_ARGS__)))
    #define qf_near(condition) __builtin_expect(condition, true)
    #define qf_far(condition)  __builtin_expect(condition, false)
  #else
    #define qf_nonnull(...)
    #define qf_near(condition) (condition)
    #define qf_far(condition)  (condition)
  #endif
  #define QF_STRUCT(name)     \
    typedef struct name name; \
    struct name
  #define QF_DISTINCT(type, name) \
    typedef struct {              \
      type value;                 \
    } name;

// types
typedef unsigned __int128 qf_u128;
typedef uint64_t qf_u64;
typedef double qf_f64;
QF_ASSERT(sizeof(qf_f64) == 8);
QF_ASSERT(sizeof(char) == 1);
#endif
