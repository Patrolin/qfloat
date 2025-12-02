#pragma once
#include <stdbool.h>
#include <stdint.h> /* IWYU pragma: keep */

// Size
#define ASSERT(condition) _Static_assert((condition), #condition)

typedef char byte;
#define byte(x) ((byte)(x))
ASSERT(sizeof(byte) == 1);
typedef uintptr_t uintptr;
#define uintptr(x) ((uintptr)(x))
typedef intptr_t intptr;
#define intptr(x) ((intptr)(x))
typedef void* rawptr;
#define rawptr(x) ((rawptr)(x))
#define Size(x) ((Size)(x))
typedef enum : uintptr {
  Byte = 1,
  KibiByte = 1024 * Byte,
  MebiByte = 1024 * KibiByte,
  GibiByte = 1024 * MebiByte,
} Size;

#define MIN(t) CONCAT(MIN_, t)
#define MAX(t) CONCAT(MAX_, t)
#define MIN_byte byte(0)
#define MAX_byte byte(255)
#define MIN_uintptr uintptr(0)
#define MAX_uintptr uintptr(-1)
#define MIN_Size Size(0)
#define MAX_Size Size(-1)
#define MIN_intptr (intptr(-1) >> intptr(1))
#define MAX_intptr intptr(-1)

// OS_xxx
#define OS_WINDOWS 0
#define OS_LINUX 0
#if _WIN32 || _WIN64
  #undef OS_WINDOWS
  #define OS_WINDOWS 1
#elif __linux__
  #undef OS_LINUX
  #define OS_LINUX 1
#endif

/* NOTE: linux forces it's own stack size... */
#define OS_MIN_STACK_SIZE (1 * MebiByte)
#define VIRTUAL_MEMORY_TO_RESERVE OS_MIN_STACK_SIZE

/* NOTE: SSD block sizes are 512B or 4KiB */
#define OS_SSD_BLOCK_SIZE_EXPONENT (9)
#define OS_SSD_BLOCK_SIZE (1 << OS_SSD_BLOCK_SIZE_EXPONENT)
ASSERT(OS_SSD_BLOCK_SIZE == 512);

#define OS_PAGE_SIZE_EXPONENT (12)
#define OS_PAGE_SIZE (1 << OS_PAGE_SIZE_EXPONENT)
ASSERT(OS_PAGE_SIZE == 4 * KibiByte);

/* NOTE: huge pages on windows requires admin permissions... */
#define OS_HUGE_PAGE_SIZE_EXPONENT (21)
#define OS_HUGE_PAGE_SIZE (1 << OS_HUGE_PAGE_SIZE_EXPONENT)
ASSERT(OS_HUGE_PAGE_SIZE == 2 * MebiByte);

// ARCH_xxx
#define ARCH_X64 0
#define ARCH_X86 0
#define ARCH_ARM64 0
#define ARCH_ARM32 0
#if __x86_64__
  #undef ARCH_X64
  #define ARCH_X64 1
#elif __i386__
  #undef ARCH_X86
  #define ARCH_X86 1
#elif __aarch64__
  #undef ARCH_ARM64
  #define ARCH_ARM64 1
#elif __arm__
  #undef ARCH_ARM32
  #define ARCH_ARM32 1
#endif
#define ARCH_IS_64_BIT (ARCH_X64 || ARCH_ARM64)
#define ARCH_IS_32_BIT (ARCH_X86 || ARCH_ARM32)
/* NOTE: stack grows downwards on almost all architectures */
#define ARCH_STACK_DIRECTION (-1)

#define ARCH_MIN_CACHE_LINE_SIZE 64
/* NOTE: macs can have bigger cache line sizes */
#define ARCH_MAX_CACHE_LINE_SIZE 128

#if __AVX512BF16__ || __ARM_FEATURE_BF16 || __has_extension(bfloat16_type)
  #define ARCH_HAS_NATIVE_BF16 1
#else
  #define ARCH_HAS_NATIVE_BF16 0
#endif
#if __HAVE_FLOAT16__ || __HAVE_FP16__ || __has_extension(c_float16)
  #define ARCH_HAS_NATIVE_F16 1
#else
  #define ARCH_HAS_NATIVE_F16 0
#endif

#if HAS_CRT
  #include <math.h>
#else
  #define fma(a, b, c) fma_impl(__COUNTER__, a, b, c)
#endif
#if ARCH_X64
  /* NOTE: windows starts aligned to 8B, while linux starts (correctly) aligned to 16B
  thus we have to realign ourselves either way... */
  #define ALIGN_STACK_POINTER() asm volatile("and rsp, -16" ::: "rsp");
  #define CALL(name) asm volatile("call " #name)
  #define cpu_relax() asm volatile("pause")
  #define fma_impl(C, a, b, c) ({         \
    f64 VAR(fma, C) = a;                  \
    asm volatile("vfmadd213sd %0, %1, %2" \
                 : "+x"(VAR(fma, C))      \
                 : "x"(b), "x"(c));       \
    VAR(fma, C);                          \
  })
  // #include <emmintrin.h>
  // #include <xmmintrin.h>
  // #include <immintrin.h>
  #undef min
  #undef max
#endif

// preprocessor helpers
#define CONCAT0(a, b) a##b
#define CONCAT(a, b) CONCAT0(a, b)
#define STR0(a) #a
#define STR(a) STR0(a)
/* NOTE: clang is stupid, and overwrites outer scope variables with the same name,
  so we need macro variables to all have different names... */
#define VAR(name, counter) CONCAT(__##name, counter)

#define IF_1(t, f) t
#define IF_0(t, f) f
#define IF(cond, t, f) CONCAT(IF_, cond)(t, f)

#define PROBE() 1, 1
#define SECOND(a, b, ...) b
/* NOTE: SECOND() is also acting like EXPAND() here... */
#define IS_PROBE(...) SECOND(__VA_ARGS__, 0)
#define IS_STRING_string PROBE()
#define IS_STRING(x) IS_PROBE(CONCAT(IS_STRING_, x))

// type keywords
#define global static
#define readonly const
#define restrict __restrict
// #define nonnull _Nonnull /* NOTE: clang doesn't have an option to assume nullable by default... */
#define align(n) __attribute__((aligned(n)))
#define vector_size(n) __attribute__((vector_size(n)))
// proc keywords
/* private to file */
#define private static
#define forward_declare
#define always_inline_ inline __attribute__((always_inline))
#define never_inline __attribute__((noinline))
#define foreign __declspec(dllimport)
// #define stdcall __attribute__((__stdcall__))
#define naked __attribute__((naked))
#define Noreturn _Noreturn void
// #define deprecated(msg) __attribute__((deprecated(msg)))
// other keywords
/* generate code that takes shorter when the condition is true, but longer when the condition is false */
#define expect_likely(condition) __builtin_expect(condition, true)
/* generate code that takes shorter when the condition is false, but longer when the condition is true */
#define expect_unlikely(condition) __builtin_expect(condition, false)
/* In the case of `if (expect_small(condition)) {x += y}`,
   expect_likely() puts the block right after and skips over it via a jump
   whereas expect_unlikely() puts the block far away, and may duplicate code on both paths.
   Should only be used if there aren't any `break` or `return` statements in the block. */
#define expect_small(condition) expect_likely(condition)
forward_declare Noreturn abort();
#define assert(condition)              \
  if (expect_unlikely(!(condition))) { \
    abort();                           \
  }
#define ASSERT_MUlTIPLE_OF(a, b) ASSERT(a % b == 0)
#define DISTINCT(type, name) \
  typedef type name

// builtins
#define offsetof(T, key) CONCAT(&(T*)(0)., key)
#define alignof(x) __alignof__(x)
#define countof(x) (intptr(sizeof(x)) / intptr(sizeof(x[0])))
always_inline_ void zero(byte* ptr, Size size) {
  for (intptr i = 0; i < size; i++) {
    ptr[size] = 0;
  }
}
#if !HAS_CRT
extern void* memset(void* ptr, int x, Size size) {
  assert(x == 0);
  zero(ptr, size);
  return ptr;
}
#endif
#define bitcast(value, t1, t2) bitcast_impl(__COUNTER__, value, t1, t2)
#define bitcast_impl(C, value, t1, t2) ({ \
  ASSERT(sizeof(t1) == sizeof(t2));       \
  t2 VAR(v, C);                           \
  *(t1*)((rawptr)(&VAR(v, C))) = value;   \
  VAR(v, C);                              \
})
#define downcast(t1, v, t2) downcast_impl(__COUNTER__, t1, v, t2)
#define downcast_impl(C, t1, v, t2) ({    \
  ASSERT(sizeof(t2) < sizeof(t1));        \
  t1 VAR(v1, C) = v;                      \
  t2 VAR(v2, C) = (t2)VAR(v1, C);         \
  assert((t1)(VAR(v2, C)) == VAR(v1, C)); \
  VAR(v2, C);                             \
})
#define saturate(t1, v1, t2) ({    \
  ASSERT(sizeof(t2) < sizeof(t1)); \
  (t2)(min(t1, v1, (t1)MAX(t2)));  \
})

// types
typedef uint64_t u64;
#define u64(x) ((u64)(x))
typedef uint32_t u32;
#define u32(x) ((u32)(x))
typedef uint16_t u16;
#define u16(x) ((u16)(x))
typedef uint8_t u8;
#define u8(x) ((u8)(x))
#define MIN_u64 u64(0)
#define MAX_u64 u64(0xffffffffffffffff)
#define MIN_u32 u32(0)
#define MAX_u32 u32(0xffffffff)
#define MIN_u16 u16(0)
#define MAX_u16 u16(0xffff)
#define MIN_u8 u8(0)
#define MAX_u8 u8(0xff)

typedef int64_t i64;
#define i64(x) ((i64)(x))
typedef int32_t i32;
#define i32(x) ((i32)(x))
typedef int16_t i16;
#define i16(x) ((i16)(x))
typedef int8_t i8;
#define i8(x) ((i8)(x))
#define MIN_i64 i64(MAX_u64)
#define MAX_i64 i64(0x7fffffffffffffff)
#define MIN_i32 i32(MAX_u32)
#define MAX_i32 i32(0x7fffffff)
#define MIN_i16 i16(MAX_u16)
#define MAX_i16 i16(0x7fff)
#define MIN_i8 i8(MAX_u8)
#define MAX_i8 i8(0x7f)

// typedef signed char CICHAR;
// typedef unsigned char CUCHAR;
// typedef short CSHORT;
// typedef unsigned short CUSHORT;
/* NOTE: 16b or 32b depending on architecture */
typedef int CINT;
#define CINT(x) ((CINT)(x))
typedef unsigned int CUINT;
#define CUINT(x) ((CUINT)(x))
// typedef long CLONG;
// typedef unsigned long CULONG;
// typedef long long CLONGLONG;
// typedef unsigned long long CULONGLONG;

typedef double f64;
#define f64(x) ((f64)(x))
ASSERT(sizeof(f64) == 8);
typedef float f32;
#define f32(x) ((f32)(x))
ASSERT(sizeof(f32) == 4);
/* NOTE: If there isn't native support, f16 is implemented by converting back and forth between f32... */
#if ARCH_HAS_NATIVE_BF16
typedef __bf16 bf16;
ASSERT(sizeof(bf16) == 2);
#endif
#if ARCH_HAS_NATIVE_F16
typedef _Float16 f16;
ASSERT(sizeof(f16) == 2);
#endif
/* NOTE: Windows is dumb */
#if OS_WINDOWS && !HAS_CRT
CINT _fltused = 0;
#else
// ASSERT(false);
#endif

// slice
/*typedef struct {
  void* ptr;
  intptr count;
} Slice;*/
typedef struct {
  byte* ptr;
  Size size;
} Bytes;
/* NOTE: utf8 string */
typedef struct {
  readonly byte* ptr;
  Size size;
} string;
/* NOTE: we take the pointer of the cstring directly to avoid a memcpy() */
#define string(const_cstr) ((string){const_cstr, sizeof(const_cstr) - 1})
#define str_slice(str, i, j) ((string){&str.ptr[i], i > j ? 0 : Size(j) - Size(i)})
bool str_equals(string a, string b) {
  if (expect_unlikely(a.size != b.size)) {
    return false;
  }
  for (intptr i = 0; i < a.size; i++) {
    if (expect_unlikely(a.ptr[i] != b.ptr[i])) {
      return false;
    }
  }
  return true;
}

// atomics: https://gcc.gnu.org/onlinedocs/gcc/_005f_005fatomic-Builtins.html
#define volatile_store(address, value) __atomic_store_n(address, value, __ATOMIC_RELAXED)
#define volatile_load(address) __atomic_load_n(address, __ATOMIC_RELAXED)
#define compiler_fence() __atomic_signal_fence(__ATOMIC_SEQ_CST)
#define mfence() __atomic_thread_fence(__ATOMIC_SEQ_CST)

#define atomic_store(address, value) __atomic_store_n(address, value, __ATOMIC_SEQ_CST)
#define atomic_load(address) __atomic_load_n(address, __ATOMIC_SEQ_CST)
#define atomic_exchange(address, value) __atomic_exchange_n(address, value, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange(address, expected, value) __atomic_compare_exchange_n(address, expected, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_compare_exchange_weak(address, expected, value) __atomic_compare_exchange_n(address, expected, value, true, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)
#define atomic_fetch_add(address, value) __atomic_fetch_add(address, value, __ATOMIC_SEQ_CST)
#define atomic_add_fetch(address, value) __atomic_add_fetch(address, value, __ATOMIC_SEQ_CST)
#define atomic_fetch_sub(address, value) __atomic_fetch_sub(address, value, __ATOMIC_SEQ_CST)
#define atomic_sub_fetch(address, value) __atomic_sub_fetch(address, value, __ATOMIC_SEQ_CST)
#define atomic_fetch_and(address, value) __atomic_fetch_and(address, value, __ATOMIC_SEQ_CST)
#define atomic_and_fetch(address, value) __atomic_and_fetch(address, value, __ATOMIC_SEQ_CST)
#define atomic_fetch_or(address, value) __atomic_fetch_or(address, value, __ATOMIC_SEQ_CST)
#define atomic_or_fetch(address, value) __atomic_or_fetch(address, value, __ATOMIC_SEQ_CST)
#define atomic_fetch_xor(address, value) __atomic_fetch_xor(address, value, __ATOMIC_SEQ_CST)
#define atomic_xor_fetch(address, value) __atomic_xor_fetch(address, value, __ATOMIC_SEQ_CST)
#define atomic_fetch_nand(address, value) __atomic_fetch_nand(address, value, __ATOMIC_SEQ_CST)
#define atomic_nand_fetch(address, value) __atomic_nand_fetch(address, value, __ATOMIC_SEQ_CST)
ASSERT(__atomic_always_lock_free(1, 0));
ASSERT(__atomic_always_lock_free(2, 0));
ASSERT(__atomic_always_lock_free(4, 0));
ASSERT(__atomic_always_lock_free(8, 0));

// bits: https://gcc.gnu.org/onlinedocs/gcc/Bit-Operation-Builtins.html
// /* AKA log2_floor() */
// #define find_first_set(t, v) find_first_set_impl(__COUNTER__, t, v)
// #define find_first_set_impl(C, t, v) ({ \
//   t VAR(value, C) = v;                  \
//   (t)(__builtin_ffsg(VAR(value, C)));   \
// })
// #define log2_ceil(t, v) log2_ceil_impl(__COUNTER__, t, v)
// #define log2_ceil_impl(C, t, v) ({                       \
//   t1 VAR(value, C) = v1;                                 \
//   VAR(value, C) <= 1 ? 0 : find_first_set((x - 1) << 1); \
// })
#define count_leading_zeros(t, v) count_leading_zeros_impl(__COUNTER__, t, v)
#define count_leading_zeros_impl(C, t, v) ({ \
  t VAR(value, C) = v;                       \
  (t)(__builtin_clzg(VAR(value, C)));        \
})
#define count_trailing_zeros(t, v) count_trailing_zeros_impl(__COUNTER__, t, v)
#define count_trailing_zeros_impl(C, t, v) ({ \
  t VAR(value, C) = v;                        \
  (t)(__builtin_ctzg(VAR(value, C)));         \
})
#define count_leading_redundant_sign_bits(t, v) count_leading_redundant_sign_bits_impl(__COUNTER__, t, v)
#define count_leading_redundant_sign_bits_impl(C, t, v) ({ \
  t VAR(value, C) = v;                                     \
  (t)(__builtin_clrsbg(VAR(value, C)));                    \
})
#define count_ones(t, v) count_ones_impl(__COUNTER__, t, v)
#define count_ones_impl(C, t, v) ({        \
  t VAR(value, C) = v;                     \
  (t)(__builtin_popcountg(VAR(value, C))); \
})
#define count_zeros(t, v) count_zeros_impl(__COUNTER__, t, v)
#define count_zeros_impl(C, t, v) ({        \
  t VAR(value, C) = v;                      \
  (t)(__builtin_popcountg(~VAR(value, C))); \
})
#define count_parity(t, v) count_parity_impl(__COUNTER__, t, v)
#define count_parity_impl(C, t, v) ({    \
  t VAR(value, C) = v;                   \
  (t)(__builtin_parityg(VAR(value, C))); \
})

// overflow: https://gcc.gnu.org/onlinedocs/gcc/Integer-Overflow-Builtins.html
#define add_overflow(a, b, dest) __builtin_add_overflow(a, b, dest)
#define sub_overflow(a, b, dest) __builtin_sub_overflow(a, b, dest)
#define mul_overflow(a, b, dest) __builtin_mul_overflow(a, b, dest)
