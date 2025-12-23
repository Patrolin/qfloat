#pragma once
#include "definitions.h"

// common
#if OS_WINDOWS
  #if NOLIBC
    #pragma comment(linker, "/ENTRY:_start")
  #endif
  #if RUN_WITHOUT_CONSOLE
    /* NOTE: /SUBSYSTEM:WINDOWS cannot connect to a console without a race condition, or spawning a new window */
    #pragma comment(linker, "/SUBSYSTEM:WINDOWS")
  #else
    #pragma comment(linker, "/SUBSYSTEM:CONSOLE")
  #endif

typedef CINT BOOL;
typedef u64 QWORD;
typedef u32 DWORD;
typedef u16 WORD;
DISTINCT(uintptr, Handle);
DISTINCT(Handle, FileHandle);
  #if ARCH_IS_64_BIT
    #define WINAPI
  #elif ARCH_IS_32_BIT
    #define WINAPI TODO
  #endif
foreign DWORD GetLastError();
foreign BOOL CloseHandle(Handle handle);
typedef enum : DWORD {
  WAIT_OBJECT_0 = 0,
  WAIT_ABANDONED = 0x80,
  WAIT_TIMEOUT = 0x102,
  WAIT_FAILED = -1,
} WaitResult;
foreign WaitResult WaitForSingleObject(Handle handle, DWORD milliseconds);
#elif OS_LINUX
  #include "os_linux.h"

/* NOTE: everything is a file on linux */
DISTINCT(CINT, FileHandle);
DISTINCT(FileHandle, Handle);
#else
ASSERT(false);
#endif

// process
#if OS_WINDOWS
typedef enum : DWORD {
  CP_UTF8 = 65001,
} CodePage;

  #pragma comment(lib, "Kernel32.lib")
foreign BOOL SetConsoleOutputCP(CodePage code_page);
foreign BOOL WriteFile(FileHandle file, rcstring buffer, DWORD buffer_size, DWORD* bytes_written, rawptr overlapped);
foreign void ExitProcess(CUINT exit_code);

#elif OS_LINUX
intptr write(FileHandle file, rcstring buffer, Size buffer_size) {
  return syscall3(SYS_write, (uintptr)file, (uintptr)buffer, buffer_size);
}
Noreturn exit_group(CINT return_code) {
  syscall1(SYS_exit_group, (uintptr)return_code);
  for (;;);
}
#else
ASSERT(false);
#endif

// process.build
#if OS_WINDOWS
typedef struct {
  DWORD nLength;
  rawptr lpSecurityDescriptor;
  BOOL bInheritHandle;
} SECURITY_ATTRIBUTES;
typedef struct {
  DWORD cb;
  rcstring lpReserved;
  rcstring lpDesktop;
  rcstring lpTitle;
  DWORD dwX;
  DWORD dwY;
  DWORD dwXSize;
  DWORD dwYSize;
  DWORD dwXCountChars;
  DWORD dwYCountChars;
  DWORD dwFillAttribute;
  DWORD dwFlags;
  WORD wShowWindow;
  WORD cbReserved2;
  rawptr lpReserved2;
  Handle hStdInput;
  Handle hStdOutput;
  Handle hStdError;
} STARTUPINFOA;
typedef struct {
  Handle hProcess;
  Handle hThread;
  DWORD dwProcessId;
  DWORD dwThreadId;
} PROCESS_INFORMATION;
foreign BOOL CreateProcessA(
    rcstring application_path,
    rcstring command,
    SECURITY_ATTRIBUTES* lpProcessAttributes,
    SECURITY_ATTRIBUTES* lpThreadAttributes,
    BOOL bInheritHandles,
    DWORD dwCreationFlags,
    rawptr lpEnvironment,
    rcstring lpCurrentDirectory,
    STARTUPINFOA* lpStartupInfo,
    PROCESS_INFORMATION* lpProcessInformation);
foreign BOOL GetExitCodeProcess(Handle hProcess, DWORD* exit_code);
DISTINCT(Handle, ModuleHandle);
foreign ModuleHandle LoadLibraryA(cstring dll_path);
rawptr GetProcAddress(ModuleHandle module, cstring proc_name);
#endif

// mem
#if OS_WINDOWS
typedef enum : DWORD {
  EXCEPTION_ACCESS_VIOLATION = 0xC0000005,
} ExceptionCode;
  #define EXCEPTION_MAXIMUM_PARAMETERS 15
typedef struct {
  ExceptionCode ExceptionCode;
  DWORD ExceptionFlags;
  struct EXCEPTION_RECORD* ExceptionRecord;
  rawptr ExceptionAddress;
  DWORD NumberParameters;
  uintptr ExceptionInformation[EXCEPTION_MAXIMUM_PARAMETERS];
} EXCEPTION_RECORD;

OPAQUE(CONTEXT);
typedef struct {
  EXCEPTION_RECORD* ExceptionRecord;
  CONTEXT* ContextRecord;
} _EXCEPTION_POINTERS;
typedef enum : DWORD {
  EXCEPTION_EXECUTE_HANDLER = 1,
  EXCEPTION_CONTINUE_SEARCH = 0,
  EXCEPTION_CONTINUE_EXECUTION = -1,
} ExceptionResult;
typedef ExceptionResult ExceptionFilter(_EXCEPTION_POINTERS* exception);

typedef enum : DWORD {
  MEM_COMMIT = 1 << 12,
  MEM_RESERVE = 1 << 13,
  MEM_DECOMMIT = 1 << 14,
  MEM_RELEASE = 1 << 15,
} AllocTypeFlags;
typedef enum : DWORD {
  PAGE_READWRITE = 1 << 2,
} AllocProtectFlags;

// foreign ExceptionFilter* SetUnhandledExceptionFilter(ExceptionFilter filter_callback);
foreign Handle AddVectoredExceptionHandler(uintptr run_first, ExceptionFilter handler);
foreign intptr VirtualAlloc(intptr address, Size size, AllocTypeFlags type_flags, AllocProtectFlags protect_flags);
foreign BOOL VirtualFree(intptr address, Size size, AllocTypeFlags type_flags);
#elif OS_LINUX
typedef enum : u32 {
  PROT_EXEC = 1 << 0,
  PROT_READ = 1 << 1,
  PROT_WRITE = 1 << 2,
} ProtectionFlags;
typedef enum : u32 {
  MAP_PRIVATE = 1 << 1,
  MAP_ANONYMOUS = 1 << 5,
  MAP_GROWSDOWN = 1 << 8,
  MAP_STACK = 1 << 17,
} AllocTypeFlags;

intptr mmap(rawptr address, Size size, ProtectionFlags protection_flags, AllocTypeFlags type_flags, FileHandle fd, Size offset) {
  return syscall6(SYS_mmap, (uintptr)address, size, protection_flags, type_flags, (uintptr)fd, offset);
}
intptr munmap(intptr address, Size size) {
  return syscall2(SYS_munmap, (uintptr)address, size);
}
#endif

// threads
#if OS_WINDOWS
typedef struct {
  union {
    DWORD dwOemId;
    struct {
      WORD wProcessorArchitecture;
      WORD wReserved;
    } DUMMYSTRUCTNAME;
  } DUMMYUNIONNAME;
  DWORD dwPageSize;
  rawptr lpMinimumApplicationAddress;
  rawptr lpMaximumApplicationAddress;
  DWORD* dwActiveProcessorMask;
  DWORD dwNumberOfProcessors;
  DWORD dwProcessorType;
  DWORD dwAllocationGranularity;
  WORD wProcessorLevel;
  WORD wProcessorRevision;
} SYSTEM_INFO;
typedef DWORD PTHREAD_START_ROUTINE(rawptr param);
typedef enum : DWORD {
  STACK_SIZE_PARAM_IS_A_RESERVATION = 0x00010000,
} CreateThreadFlags;
DISTINCT(Handle, ThreadHandle);
typedef enum : DWORD {
  INFINITE = -1,
} OsTimerMillis;

  #pragma comment(lib, "Synchronization.lib")
foreign void GetSystemInfo(SYSTEM_INFO* lpSystemInfo);
foreign ThreadHandle CreateThread(
    SECURITY_ATTRIBUTES* security,
    Size stack_size,
    PTHREAD_START_ROUTINE start_proc,
    rawptr param,
    DWORD flags,
    DWORD* thread_id);
foreign void WaitOnAddress(volatile rawptr address, rawptr while_value, Size address_size, DWORD timeout);
foreign void WakeByAddressAll(rawptr address);
#elif OS_LINUX
typedef CINT pid_t;
typedef u64 rlim_t;
typedef enum : CUINT {
  RLIMIT_STACK = 3,
} ResourceType;
typedef struct {
  rlim_t rlim_cur;
  rlim_t rlim_max;
} rlimit;
intptr getrlimit(ResourceType key, rlimit* value) {
  return syscall2(SYS_getrlimit, key, (uintptr)value);
}
intptr sched_getaffinity(pid_t pid, Size masks_size, u8* masks) {
  return syscall3(SYS_sched_getaffinity, (uintptr)pid, masks_size, (uintptr)masks);
};

typedef enum : CUINT {
  CLONE_VM = 1 << 8,
  CLONE_FS = 1 << 9,
  CLONE_FILES = 1 << 10,
  CLONE_SIGHAND = 1 << 11,
  CLONE_PARENT = 1 << 15,
  CLONE_THREAD = 1 < 16,
  CLONE_SYSVSEM = 1 << 18,
  CLONE_IO = 1 < 31,
} ThreadFlags;
typedef enum : u64 {
  SIGABRT = 6,
  SIGKILL = 9,
  SIGCHLD = 17,
} SignalType;
// https://nullprogram.com/blog/2023/03/23/
typedef CUINT _linux_thread_entry(rawptr);
typedef align(16) struct {
  _linux_thread_entry* entry;
  rawptr param;
} new_thread_data;
naked intptr newthread(ThreadFlags flags, new_thread_data* stack) {
  #if ARCH_X64
  asm volatile(
      "mov eax, 56\n"  // rax = SYS_clone
      "syscall\n"
      "mov rdi, [rsp+8]\n"
      "ret" ::: "rcx", "r11", "memory", "rax", "rdi", "rsi");
  #else
  assert(false);
  #endif
}

typedef enum : CINT {
  FUTEX_WAIT = 0,
  FUTEX_WAKE = 1,
} FutexOp;
typedef intptr time_t;
typedef struct {
  time_t t_sec;
  time_t t_nsec;
} timespec;
intptr futex_wait(u32* address, u32 while_value, readonly timespec* timeout) {
  return syscall4(SYS_futex, (uintptr)address, FUTEX_WAIT, while_value, (uintptr)timeout);
}
intptr futex_wake(u32* address, u32 count_to_wake) {
  return syscall3(SYS_futex, (uintptr)address, FUTEX_WAKE, count_to_wake);
}
#else
// ASSERT(false);
#endif

// file
#if OS_WINDOWS
/* TODO: file api */
#elif OS_LINUX
typedef enum : CUINT {
  O_WRONLY = 1 << 0,
  O_RDWR = 1 << 1,
  /* create if not exists */
  O_CREAT = 1 << 6,
  /* don't open */
  O_EXCL = 1 << 7,
  /* truncate */
  O_TRUNC = 1 << 9,
  O_DIRECTORY = 1 << 16,
} FileFlags;
intptr open(rcstring path, FileFlags flags, CUINT mode) {
  return syscall3(SYS_open, (uintptr)path, flags, mode);
}
#endif
