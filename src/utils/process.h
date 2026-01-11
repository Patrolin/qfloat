#pragma once
#include "definitions.h"
#include "fmt.h"
#include "os.h"
#include "mem.h"

// init
#if OS_WINDOWS
typedef enum : DWORD {
  CP_UTF8 = 65001,
} CodePage;

foreign BOOL WINAPI SetConsoleOutputCP(CodePage code_page);
#elif OS_LINUX
#else
ASSERT(false);
#endif

void _init_console() {
#if OS_WINDOWS
  SetConsoleOutputCP(CP_UTF8);
#else
  // ASSERT(false);
#endif
}
forward_declare void _init_page_fault_handler();
void _init_shared_arena() {
  Bytes buffer = page_reserve(VIRTUAL_MEMORY_TO_RESERVE);
  global_arena = arena_allocator(buffer);
}

// exit
#if OS_WINDOWS
foreign void ExitProcess(CUINT exit_code);
#elif OS_LINUX
noreturn_ exit_group(CINT return_code) {
  syscall1(SYS_exit_group, (uintptr)return_code);
}
#endif

noreturn_ exit_process(CINT exit_code) {
#if OS_WINDOWS
  ExitProcess((CUINT)exit_code);
#elif OS_LINUX
  exit_group(exit_code);
#else
  ASSERT(false);
#endif
  for (;;);
}
noreturn_ abort() {
  // TODO: maybe just use `trap()`?
  exit_process(1);
}

// entry
#if SINGLE_CORE
forward_declare void main_singlecore();
#else
forward_declare void _init_threads();
#endif
noreturn_ _init_process() {
#if OS_WINDOWS && NOLIBC
  asm volatile("" ::"m"(_fltused));
#endif
  _init_console();
  _init_page_fault_handler();
  _init_shared_arena();
#if SINGLE_CORE
  main_singlecore();
#else
  _init_threads();
#endif
  exit_process(0);
}
#if NOLIBC
  /* NOTE: windows starts aligned to 8B, while linux starts (correctly) aligned to 16B
    thus we have to realign the stack pointer either way... */
  #if ARCH_X64
    #define _start_impl() asm volatile("xor ebp, ebp; and rsp, -16; call _start_process" ::: "rbp", "rsp");
  #else
ASSERT(false);
  #endif
naked noreturn_ _start() {
  _start_impl();
}
#else
CINT main() {
  _init_process();
}
#endif

// build system
#if OS_WINDOWS
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
  cstring command,
  readonly SECURITY_ATTRIBUTES *process_security,
  readonly SECURITY_ATTRIBUTES *thread_security,
  BOOL inherit_handles,
  DWORD creation_flags,
  readonly rawptr env,
  rcstring current_dir,
  readonly STARTUPINFOA *startup_info,
  PROCESS_INFORMATION *process_info);
foreign BOOL GetExitCodeProcess(Handle hProcess, DWORD *exit_code);
DISTINCT(Handle, ModuleHandle);
foreign ModuleHandle LoadLibraryA(rcstring dll_path);
rawptr GetProcAddress(ModuleHandle module, cstring proc_name);
#endif

typedef struct {
  string *start;
  Size count;
} BuildArgs;
#define arg_alloc(args, arg) arg_alloc_impl(args, string(arg))
#define arg_alloc2(args, arg1, arg2)  \
  arg_alloc_impl(args, string(arg1)); \
  arg_alloc_impl(args, string(arg2))
static void arg_alloc_impl(BuildArgs *restrict args, string arg) {
  /* TODO: maybe just implement a dynamic array? */
  string *ptr = (string *)atomic_fetch_add(&global_arena->next, sizeof(string));
  *ptr = arg;
  args->start = args->start == 0 ? ptr : args->start;
  assert(ptr == &args->start[args->count]); // assert single-threaded
  args->count += 1;
}
#if BUILD_SYSTEM
/* NOTE: we need to have the alignment at compile time, so the optimizer can make better decisions */
uintptr _get_cache_alignment() {
  #if OS_WINDOWS
  // TODO: `GetLogicalProcessorInformationEx().Buffer[i].LineSize`
  uintptr cache_line_size = 0;
  #elif OS_LINUX
  ASSERT(false); // TODO: `max(grep cache_alignment /proc/cpuinfo)`
  #else
  ASSERT(false);
  #endif
  return cache_line_size;
}
#endif

#define run_process(app, args) run_process_impl(string(app), args)
static void run_process_impl(readonly string app, readonly BuildArgs *args) {
#if OS_WINDOWS
  // copy to a single cstring
  byte *command = (byte *)global_arena->next;
  memcpy(command, app.ptr, app.size);
  byte *next = command + app.size;
  if (expect_near(args != 0)) {
    for (intptr i = 0; i < args->count; i++) {
      string str = args->start[i];
      *(next++) = ' ';
      memcpy(next, str.ptr, str.size);
      next += str.size;
    }
  }
  (*next) = '\n';
  string command_str = (string){command, (Size)(next - command + 1)};
  (*next) = '\0';
  // start new process
  STARTUPINFOA startup_info = {
    .cb = sizeof(STARTUPINFOA),
  };
  PROCESS_INFORMATION process_info;
  println(string, command_str);
  bool ok = CreateProcessA(0, command, 0, 0, false, 0, 0, 0, &startup_info, &process_info);
  if (!ok) {
    DWORD err = GetLastError();
    printfln2(string("err: %, ok: %"), u32, err, bool, ok);
  }
  assert(ok);
  WaitResult wait_result = WaitForSingleObject(process_info.hProcess, TIME_INFINITE);
  assert(wait_result == WAIT_OBJECT_0);
  u32 exit_code;
  GetExitCodeProcess(process_info.hProcess, &exit_code);
  if (exit_code != 0) exit_process((CINT)exit_code);

  CloseHandle(process_info.hThread);
  CloseHandle(process_info.hProcess);
#else
  // TODO: In a new thread: `execve(command, args, 0);`
  ASSERT(false);
#endif
  // assert single-threaded
  assert(atomic_compare_exchange(&global_arena->next, (intptr *)&command, (intptr)command));
}
