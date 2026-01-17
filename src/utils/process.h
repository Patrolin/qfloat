#pragma once
#include "builtin.h"
#include "fmt.h"
#include "os.h"
#include "mem.h"

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
  if (expect_far(args->start == 0)) {
    intptr start = align_up(global_arena->next, alignof(string));
    args->start = (string *)start;
    global_arena->next = start;
  }
  string *ptr = (string *)atomic_fetch_add(&global_arena->next, sizeof(string));
  *ptr = arg;
  // assert(!out_of_memory && single_threaded)
  assert(intptr(ptr) + intptr(sizeof(string)) < global_arena->end && ptr == &args->start[args->count]);
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
