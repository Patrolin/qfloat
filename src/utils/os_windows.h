#pragma once
#include "definitions.h"

// common
typedef CINT BOOL;
typedef u64 QWORD;
typedef u32 DWORD;
typedef u16 WORD;
#if ARCH_IS_64_BIT
  #define WINAPI
#elif ARCH_IS_32_BIT
  #define WINAPI TODO
#endif

DISTINCT(uintptr, Handle);
DISTINCT(Handle, FileHandle);
#define INVALID_HANDLE (Handle)(-1)
foreign bool CloseHandle(Handle handle);
foreign bool WriteFile(FileHandle file, rcstring buffer, DWORD buffer_size, DWORD *bytes_written, rawptr overlapped);

// linker flags
#if NOLIBC
  #pragma comment(linker, "/ENTRY:_start")
#endif
#if RUN_WITHOUT_CONSOLE
  /* NOTE: /SUBSYSTEM:WINDOWS cannot connect to a console without a race condition, or spawning a new window */
  #pragma comment(linker, "/SUBSYSTEM:WINDOWS")
#else
  #pragma comment(linker, "/SUBSYSTEM:CONSOLE")
#endif
#pragma comment(lib, "Kernel32.lib")

// types
typedef struct {
  DWORD nLength;
  rawptr lpSecurityDescriptor;
  BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;
#define TIME_INFINITE (DWORD)(-1)
typedef enum : DWORD {
  WAIT_OBJECT_0 = 0,
  WAIT_ABANDONED = 0x80,
  WAIT_TIMEOUT = 0x102,
  WAIT_FAILED = -1,
} WaitResult;

// utils
foreign DWORD GetLastError();
foreign WaitResult WaitForSingleObject(Handle handle, DWORD milliseconds);
