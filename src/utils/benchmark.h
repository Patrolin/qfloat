#pragma once
#include "builtin.h"

#define optimizer_fence(value) asm volatile("" : "+X"(value))
// TODO: timings
