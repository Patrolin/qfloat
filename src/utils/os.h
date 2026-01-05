#pragma once
#include "definitions.h"
// OneLineFormatOffRegex: ^(// NOLINT|logger$)

/* IWYU pragma: begin_exports */
#if OS_WINDOWS
  #include "os_windows.h"
#elif OS_LINUX
  #include "os_linux.h"
#else
ASSERT(false); // NOLINT
#endif
/* IWYU pragma: end_exports */
