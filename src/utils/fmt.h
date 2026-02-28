#pragma once
#include "builtin.h"
#include "os.h"

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
  if (expect_far(bytes_written != str.size)) {
    abort();
  }
}

// sprint()
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
#define sprint(t1, v1, ptr_end)         CONCAT(sprint_, t1)(v1, ptr_end)
#define sprint_to_string(ptr_end, size) ((string){ptr_end - iptr(size), size})

#define sprint_size_string(value) (value.size)
usize sprint_string(string str, byte *buffer_end) {
  byte *buffer = buffer_end - str.size;
  for (iptr i = 0; i < str.size; i++) {
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
  iptr i = 0;
  do {
    iptr digit = value % 10;
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

#define sprint_size_hex_pad(value) (2 + 2 * sizeof(u64))
byte *HEX_DIGITS = "0123456789ABCDEF";
usize sprint_hex_pad(u64 value, byte *buffer_end) {
  iptr i = 0;
  do {
    u64 digit = value & 0xf;
    value = value >> 4;
    buffer_end[--i] = HEX_DIGITS[digit];
  } while (i > -2 * iptr(sizeof(u64)));
  buffer_end[--i] = 'x';
  buffer_end[--i] = '0';
  return usize(-i);
}
#define sprint_size_hex(value)        (2 + 2 * sizeof(u64))
#define sprint_hex(value, buffer_end) sprint_hex_impl((u64)value, buffer_end)
usize sprint_hex_impl(u64 value, byte *buffer_end) {
  iptr i = 0;
  do {
    u64 digit = value & 0xf;
    value = value >> 4;
    buffer_end[--i] = HEX_DIGITS[digit];
  } while (value != 0);
  buffer_end[--i] = 'x';
  buffer_end[--i] = '0';
  return usize(-i);
}
#define sprint_size_fhex(value)        sprint_size_uhex(value)
#define sprint_fhex(value, buffer_end) sprint_uhex(bitcast(value, f64, u64), buffer_end)

#if ARCH_IS_64_BIT
  #define sprint_size_uptr(value) sprint_size_u64(value)
usize sprint_uptr(uptr value, byte *buffer_end) {
  return sprint_u64(u64(value), buffer_end);
}
  #define sprint_size_iptr(value) sprint_size_u64(value)
usize sprint_iptr(iptr value, byte *buffer_end) {
  return sprint_u64(u64(value), buffer_end);
}
#elif ARCH_IS_32_BIT
  #define sprint_size_uptr(value) sprint_size_u32(value)
usize sprint_uptr(uptr value, byte *buffer_end) {
  return sprint_u32(u32(value), buffer_end);
}
  #define sprint_size_iptr(value) sprint_size_u32(value)
usize sprint_iptr(iptr value, byte *buffer_end) {
  return sprint_u32(u32(value), buffer_end);
}
#endif
#define sprint_size_usize(value) sprint_size_uptr(value)
usize sprint_usize(usize value, byte *buffer_end) {
  return sprint_uptr(value, buffer_end);
}
#define sprint_size_isize(value) sprint_size_iptr(value)
usize sprint_isize(isize value, byte *buffer_end) {
  return sprint_iptr(value, buffer_end);
}

// sprintf()
#define sprintf1(ptr_end, format, t1, v1)         sprintf1_impl(__COUNTER__, ptr_end, format, t1, v1)
#define sprintf1_impl(C, ptr_end, format, t1, v1) ({                           \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                       \
  iptr VAR(i, C) = iptr(VAR(fmt, C).size);                                 \
  iptr VAR(j, C) = VAR(i, C);                                                \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end);                  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t1, v1, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint(string, VAR(before, C), ptr_end - VAR(size, C));    \
  }                                                                            \
  VAR(size, C);                                                                \
})
#define sprintf2(ptr_end, format, t1, v1, t2, v2)         sprintf2_impl(__COUNTER__, ptr_end, format, t1, v1, t2, v2)
#define sprintf2_impl(C, ptr_end, format, t1, v1, t2, v2) ({                   \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                       \
  iptr VAR(i, C) = iptr(VAR(fmt, C).size);                                 \
  iptr VAR(j, C) = VAR(i, C);                                                \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end);                  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t2, v2, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end - VAR(size, C));   \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t1, v1, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint(string, VAR(before, C), ptr_end - VAR(size, C));    \
  }                                                                            \
  VAR(size, C);                                                                \
})
#define sprintf3(ptr_end, format, t1, v1, t2, v2, t3, v3)         sprintf3_impl(__COUNTER__, ptr_end, format, t1, v1, t2, v2, t3, v3)
#define sprintf3_impl(C, ptr_end, format, t1, v1, t2, v2, t3, v3) ({           \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                       \
  iptr VAR(i, C) = iptr(VAR(fmt, C).size);                                 \
  iptr VAR(j, C) = VAR(i, C);                                                \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end);                  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t3, v3, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end - VAR(size, C));   \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t2, v2, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end - VAR(size, C));   \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t1, v1, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint(string, VAR(before, C), ptr_end - VAR(size, C));    \
  }                                                                            \
  VAR(size, C);                                                                \
})
#define sprintf4(ptr_end, format, t1, v1, t2, v2, t3, v3, t4, v4)         sprintf4_impl(__COUNTER__, ptr_end, format, t1, v1, t2, v2, t3, v3, t4, v4)
#define sprintf4_impl(C, ptr_end, format, t1, v1, t2, v2, t3, v3, t4, v4) ({   \
  string VAR(fmt, C) = format;                                                 \
  usize VAR(size, C) = 0;                                                       \
  iptr VAR(i, C) = iptr(VAR(fmt, C).size);                                 \
  iptr VAR(j, C) = VAR(i, C);                                                \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end);                  \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t4, v4, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end - VAR(size, C));   \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t3, v3, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end - VAR(size, C));   \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t2, v2, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  while (VAR(i, C) > 0) {                                                      \
    (VAR(i, C)--);                                                             \
    if (expect_far(VAR(fmt, C).ptr[VAR(i, C)] == '%')) {                       \
      string VAR(after, C) = str_slice(VAR(fmt, C), VAR(i, C) + 1, VAR(j, C)); \
      VAR(size, C) += sprint(string, VAR(after, C), ptr_end - VAR(size, C));   \
      VAR(j, C) = VAR(i, C);                                                   \
      VAR(size, C) += sprint(t1, v1, ptr_end - VAR(size, C));                  \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (expect_near(VAR(j, C) > 0)) {                                            \
    string VAR(before, C) = str_slice(VAR(fmt, C), 0, VAR(j, C));              \
    VAR(size, C) += sprint(string, VAR(before, C), ptr_end - VAR(size, C));    \
  }                                                                            \
  VAR(size, C);                                                                \
})

// print()
void print_string(string str) {
  fprint(STDOUT, str);
}
#define print_copy(t1, v1)         print_copy_impl(__COUNTER__, t1, v1)
#define print_copy_impl(C, t1, v1) ({                                   \
  usize VAR(max_size, C) = sprint_size1(t1, v1);                         \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));      \
                                                                        \
  usize VAR(size, C) = CONCAT(sprint_, t1)(v1, VAR(ptr_end, C));         \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C)); \
  print_string(VAR(msg, C));                                            \
})
#define print(t1, v1)           IF(IS_STRING(t1), print_string(v1), print_copy(t1, v1))
#define println(t1, v1)         println_impl(__COUNTER__, t1, v1)
#define println_impl(C, t1, v1) ({                                      \
  usize VAR(max_size, C) = sprint_size1(t1, v1) + 1;                     \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));      \
                                                                        \
  *(VAR(ptr_end, C) - 1) = '\n';                                        \
  usize VAR(size, C) = CONCAT(sprint_, t1)(v1, VAR(ptr_end, C) - 1) + 1; \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C)); \
  print_string(VAR(msg, C));                                            \
})

// printf()
#define printf1(format, t1, v1)         printf1_impl(__COUNTER__, format, t1, v1)
#define printf1_impl(C, format, t1, v1) ({                              \
  usize VAR(max_size, C) = sprint_size2(string, format, t1, v1);         \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));      \
                                                                        \
  usize VAR(size, C) = sprintf1(VAR(ptr_end, C), format, t1, v1);        \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C)); \
  print_string(VAR(msg, C));                                            \
})
#define printf2(format, t1, v1, t2, v2)         printf2_impl(__COUNTER__, format, t1, v1, t2, v2)
#define printf2_impl(C, format, t1, v1, t2, v2) ({                       \
  usize VAR(max_size, C) = sprint_size3(string, format, t1, v1, t2, v2);  \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));       \
                                                                         \
  usize VAR(size, C) = sprintf2(VAR(ptr_end, C), format, t1, v1, t2, v2); \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C));  \
  print_string(VAR(msg, C));                                             \
})
#define printf3(format, t1, v1, t2, v2, t3, v3)         printf3_impl(__COUNTER__, format, t1, v1, t2, v2, t3, v3)
#define printf3_impl(C, format, t1, v1, t2, v2, t3, v3) ({                       \
  usize VAR(max_size, C) = sprint_size4(string, format, t1, v1, t2, v2, t3, v3);  \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));               \
                                                                                 \
  usize VAR(size, C) = sprintf3(VAR(ptr_end, C), format, t1, v1, t2, v2, t3, v3); \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C));          \
  print_string(VAR(msg, C));                                                     \
})

// printfln()
#define printfln1(format, t1, v1)         printfln1_impl(__COUNTER__, format, t1, v1)
#define printfln1_impl(C, format, t1, v1) ({                             \
  usize VAR(max_size, C) = sprint_size2(string, format, t1, v1) + 1;      \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));       \
                                                                         \
  *(VAR(ptr_end, C) - 1) = '\n';                                         \
  usize VAR(size, C) = sprintf1(VAR(ptr_end, C) - 1, format, t1, v1) + 1; \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C));  \
  print_string(VAR(msg, C));                                             \
})
#define printfln2(format, t1, v1, t2, v2)         printfln2_impl(__COUNTER__, format, t1, v1, t2, v2)
#define printfln2_impl(C, format, t1, v1, t2, v2) ({                             \
  usize VAR(max_size, C) = sprint_size3(string, format, t1, v1, t2, v2) + 1;      \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));               \
                                                                                 \
  *(VAR(ptr_end, C) - 1) = '\n';                                                 \
  usize VAR(size, C) = sprintf2(VAR(ptr_end, C) - 1, format, t1, v1, t2, v2) + 1; \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C));          \
  print_string(VAR(msg, C));                                                     \
})
#define printfln3(format, t1, v1, t2, v2, t3, v3)         printfln3_impl(__COUNTER__, format, t1, v1, t2, v2, t3, v3)
#define printfln3_impl(C, format, t1, v1, t2, v2, t3, v3) ({                             \
  usize VAR(max_size, C) = sprint_size4(string, format, t1, v1, t2, v2, t3, v3) + 1;      \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));                       \
                                                                                         \
  *(VAR(ptr_end, C) - 1) = '\n';                                                         \
  usize VAR(size, C) = sprintf3(VAR(ptr_end, C) - 1, format, t1, v1, t2, v2, t3, v3) + 1; \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C));                  \
  print_string(VAR(msg, C));                                                             \
})
#define printfln4(format, t1, v1, t2, v2, t3, v3, t4, v4)         printfln4_impl(__COUNTER__, format, t1, v1, t2, v2, t3, v3, t4, v4)
#define printfln4_impl(C, format, t1, v1, t2, v2, t3, v3, t4, v4) ({                             \
  usize VAR(max_size, C) = sprint_size5(string, format, t1, v1, t2, v2, t3, v3, t4, v4) + 1;      \
  STACK_BUFFER(VAR(buffer, C), VAR(max_size, C), VAR(ptr_end, C));                               \
                                                                                                 \
  *(VAR(ptr_end, C) - 1) = '\n';                                                                 \
  usize VAR(size, C) = sprintf4(VAR(ptr_end, C) - 1, format, t1, v1, t2, v2, t3, v3, t4, v4) + 1; \
  string VAR(msg, C) = sprint_to_string(VAR(ptr_end, C), VAR(size, C));                          \
  print_string(VAR(msg, C));                                                                     \
})

// TODO: maybe use __builtin_dump_struct() for structs?
