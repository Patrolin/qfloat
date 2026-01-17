#pragma once
#include "builtin.h"

/* IWYU pragma: begin_exports */
#if OS_WINDOWS
  #include "os_windows.h"
#elif OS_LINUX
  #include "os_linux.h"
#else
ASSERT(false);
#endif
/* IWYU pragma: end_exports */
