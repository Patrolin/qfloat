#pragma once
#include "builtin.h"
#include "os.h"

// syscalls
#if OS_WINDOWS
typedef enum : DWORD {
  CP_UTF8 = 65001,
} CodePage;

foreign BOOL WINAPI SetConsoleOutputCP(CodePage code_page);
#elif OS_LINUX
#else
ASSERT(false);
#endif

// init
void _init_console() {
#if OS_WINDOWS
  SetConsoleOutputCP(CP_UTF8);
#else
  // ASSERT(false);
#endif
}
#if SINGLE_CORE
forward_declare void main_singlecore();
#endif

// fprint()
#define DELETE_LINE "\x1b[2K\r"
#define NEWLINE     "\n"

void fprint(FileHandle file, string str) {
#if OS_WINDOWS
  DWORD bytes_written;
  DWORD chars_to_write = downcast(usize, str.size, DWORD);
  WriteFile(file, str.ptr, chars_to_write, &bytes_written, 0);
#elif OS_LINUX
  iptr bytes_written = write(file, str.ptr, str.size);
#else
  abort();
#endif
  if (expect_far(bytes_written != str.size)) abort();
}

// sprint_size(), sprint1()
#define INVALID_SPRINT_SIZE(...)                     ASSERT(false, "Invalid argument count for sprint_size()")
#define sprint_size(...)                             OVERLOAD11(__VA_ARGS__ __VA_OPT__(, ) sprint_size5, INVALID_SPRINT_SIZE, sprint_size4, INVALID_SPRINT_SIZE, sprint_size3, INVALID_SPRINT_SIZE, sprint_size2, INVALID_SPRINT_SIZE, sprint_size1, INVALID_SPRINT_SIZE, INVALID_SPRINT_SIZE)(__VA_ARGS__)
#define sprint_size1(t1, v1)                         (CONCAT(sprint_size_, t1)(v1))
#define sprint_size2(t1, v1, t2, v2)                 (CONCAT(sprint_size_, t1)(v1) + CONCAT(sprint_size_, t2)(v2))
#define sprint_size3(t1, v1, t2, v2, t3, v3)         (CONCAT(sprint_size_, t1)(v1) + CONCAT(sprint_size_, t2)(v2) + CONCAT(sprint_size_, t3)(v3))
#define sprint_size4(t1, v1, t2, v2, t3, v3, t4, v4) (CONCAT(sprint_size_, t1)(v1) + CONCAT(sprint_size_, t2)(v2) \
                                                      + CONCAT(sprint_size_, t3)(v3) + CONCAT(sprint_size_, t4)(v4))
#define sprint_size5(t1, v1, t2, v2, t3, v3, t4, v4, t5, v5) (CONCAT(sprint_size_, t1)(v1) + CONCAT(sprint_size_, t2)(v2) \
                                                              + CONCAT(sprint_size_, t3)(v3) + CONCAT(sprint_size_, t4)(v4) + CONCAT(sprint_size_, t5)(v5))
#define STACK_BUFFER(buffer, max_size, ptr_end) \
  byte buffer[max_size];                        \
  byte *ptr_end = &buffer[max_size]
#define sprint1(t1, v1, ptr_end)        CONCAT(sprint_, t1)(v1, ptr_end)
#define sprint_to_string(ptr_end, size) ((string){ptr_end - iptr(size), size})

#define sprint_size_string(value) (value.size)
usize sprint_string(string str, byte *buffer_end) {
  byte *buffer = buffer_end - str.size;
  for (usize i = 0; i < str.size; i++) {
    buffer[i] = str.ptr[i];
  }
  return str.size;
}

#define sprint_size__Bool(value)        sprint_size_bool(value)
#define sprint__Bool(value, buffer_end) sprint_bool(value, buffer_end)
#define sprint_size_bool(value)         5
usize sprint_bool(bool value, byte *buffer_end) {
  string msg = value ? string("true") : string("false");
  return sprint_string(msg, buffer_end);
}

/* NOTE: log10_ceil(max(type)) == log10_ceil(pow(2, bits) - 1) */
#define sprint_size_u64(value)  (20)
#define sprint_size_u32(value)  (10)
#define sprint_size_u16(value)  (5)
#define sprint_size_u8(value)   (3)
#define sprint_size_byte(value) (3)
usize sprint_u64(u64 value, byte *buffer_end) {
  isize i = 0;
  do {
    isize digit = value % 10;
    value = value / 10;
    buffer_end[--i] = '0' + (byte)digit;
  } while (value != 0);
  return usize(-i);
}
usize sprint_u32(u32 value, byte *buffer_end) {
  return sprint_u64(value, buffer_end);
}
usize sprint_u16(u16 value, byte *buffer_end) {
  return sprint_u64(value, buffer_end);
}
usize sprint_u8(u8 value, byte *buffer_end) {
  return sprint_u64(value, buffer_end);
}
usize sprint_byte(u8 value, byte *buffer_end) {
  return sprint_u64(value, buffer_end);
}

/* NOTE: 1 + log10_ceil(-min(type)) == 1 + log10_ceil(pow(2, bits-1)) */
#define sprint_size_i64(value) (20)
#define sprint_size_i32(value) (11)
#define sprint_size_i16(value) (6)
#define sprint_size_i8(value)  (4)
usize sprint_i64(i64 value, byte *buffer_end) {
  u64 value_abs = u64(value);
  if (value_abs > u64(MAX(i64))) value_abs = -value_abs;
  usize size = sprint_u64(value_abs, buffer_end);
  if (expect_near(value < 0)) {
    *(buffer_end - size - 1) = '-';
    size += 1;
  }
  return size;
}
usize sprint_i32(i32 value, byte *buffer_end) {
  return sprint_i64(value, buffer_end);
}
usize sprint_i16(i16 value, byte *buffer_end) {
  return sprint_i64(value, buffer_end);
}
usize sprint_i8(i8 value, byte *buffer_end) {
  return sprint_i64(value, buffer_end);
}

#define sprint_size_uptr(value) (2 + 2 * sizeof(uptr))
byte *HEX_DIGITS = "0123456789ABCDEF";
usize sprint_uptr(uptr value, byte *buffer_end) {
  isize i = 0;
  do {
    uptr digit = value & 0xf;
    value = value >> 4;
    buffer_end[--i] = HEX_DIGITS[digit];
  } while (i > -2 * isize(sizeof(u64)));
  buffer_end[--i] = 'x';
  buffer_end[--i] = '0';
  return usize(-i);
}
#define sprint_size_hex(value)        (2 + 2 * sizeof(u64))
#define sprint_hex(value, buffer_end) sprint_hex_impl((u64)value, buffer_end)
usize sprint_hex_impl(u64 value, byte *buffer_end) {
  isize i = 0;
  do {
    u64 digit = value & 0xf;
    value = value >> 4;
    buffer_end[--i] = HEX_DIGITS[digit];
  } while (value != 0);
  buffer_end[--i] = 'x';
  buffer_end[--i] = '0';
  return usize(-i);
}

#define sprint_size_usize(value) sprint_size_u64(value)
usize sprint_usize(usize value, byte *buffer_end) {
  return sprint_u64(value, buffer_end);
}
#define sprint_size_isize(value) sprint_size_i64(value)
usize sprint_isize(isize value, byte *buffer_end) {
  return sprint_i64(value, buffer_end);
}

// sprintf()
#define INVALID_SPRINTF(...)                      ASSERT(false, "Invalid argument count for sprintf()")
#define sprintf(ptr_end, format, ...)             OVERLOAD9(__VA_ARGS__ __VA_OPT__(, ) sprintf4, INVALID_SPRINTF, sprintf3, INVALID_SPRINTF, sprintf2, INVALID_SPRINTF, sprintf1, INVALID_SPRINTF, INVALID_SPRINTF)(ptr_end, format, __VA_ARGS__)
#define sprintf1(ptr_end, format, t1, v1)         sprintf1_impl(__COUNTER__, ptr_end, format, t1, v1)
#define sprintf1_impl(C, ptr_end, format, t1, v1) ({                           \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                      \
  isize VAR(i, C) = isize(VAR(fmt, C).size);                                   \
  isize VAR(j, C) = VAR(i, C);                                                 \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end);                 \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t1, v1, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint1(string, VAR(before, C), ptr_end - VAR(size, C));   \
  }                                                                            \
  VAR(size, C);                                                                \
})
#define sprintf2(ptr_end, format, t1, v1, t2, v2)         sprintf2_impl(__COUNTER__, ptr_end, format, t1, v1, t2, v2)
#define sprintf2_impl(C, ptr_end, format, t1, v1, t2, v2) ({                   \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                      \
  isize VAR(i, C) = isize(VAR(fmt, C).size);                                   \
  isize VAR(j, C) = VAR(i, C);                                                 \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end);                 \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t2, v2, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end - VAR(size, C));  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t1, v1, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint1(string, VAR(before, C), ptr_end - VAR(size, C));   \
  }                                                                            \
  VAR(size, C);                                                                \
})
#define sprintf3(ptr_end, format, t1, v1, t2, v2, t3, v3)         sprintf3_impl(__COUNTER__, ptr_end, format, t1, v1, t2, v2, t3, v3)
#define sprintf3_impl(C, ptr_end, format, t1, v1, t2, v2, t3, v3) ({           \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                      \
  isize VAR(i, C) = isize(VAR(fmt, C).size);                                   \
  isize VAR(j, C) = VAR(i, C);                                                 \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end);                 \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t3, v3, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end - VAR(size, C));  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t2, v2, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end - VAR(size, C));  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t1, v1, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint1(string, VAR(before, C), ptr_end - VAR(size, C));   \
  }                                                                            \
  VAR(size, C);                                                                \
})
#define sprintf4(ptr_end, format, t1, v1, t2, v2, t3, v3, t4, v4)         sprintf4_impl(__COUNTER__, ptr_end, format, t1, v1, t2, v2, t3, v3, t4, v4)
#define sprintf4_impl(C, ptr_end, format, t1, v1, t2, v2, t3, v3, t4, v4) ({   \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                      \
  isize VAR(i, C) = isize(VAR(fmt, C).size);                                   \
  isize VAR(j, C) = VAR(i, C);                                                 \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end);                 \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t4, v4, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end - VAR(size, C));  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t3, v3, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end - VAR(size, C));  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t2, v2, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint1(string, VAR(after, C), ptr_end - VAR(size, C));  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint1(t1, v1, ptr_end - VAR(size, C));                 \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint1(string, VAR(before, C), ptr_end - VAR(size, C));   \
  }                                                                            \
  VAR(size, C);                                                                \
})

// print(), println()
#define print_string(str)   fprint(STDOUT, str)
#define println_string(str) printf("%\n", string, str)
#define print(rcstr)        print_string(string(rcstr))
#define println(rcstr)      print_string(string(rcstr "\n"))

// printf(), printfln()
#define printf_impl(format, ...)                                       \
  do {                                                                 \
    usize printf_max_size = sprint_size(string, format, __VA_ARGS__);  \
    STACK_BUFFER(printf_buffer, printf_max_size, printf_ptr_end);      \
    usize printf_size = sprintf(printf_ptr_end, format, __VA_ARGS__);  \
    string printf_msg = sprint_to_string(printf_ptr_end, printf_size); \
    print_string(printf_msg);                                          \
  } while (0)
#define printf_string(format_str, ...) printf_impl(format_str __VA_OPT__(, ) __VA_ARGS__)
#define printf(format_rcstr, ...)      printf_string(string(format_rcstr) __VA_OPT__(, ) __VA_ARGS__)
#define printfln(format_rcstr, ...)    printf(format_rcstr "\n" __VA_OPT__(, ) __VA_ARGS__)

// TODO: maybe use __builtin_dump_struct() for structs?
