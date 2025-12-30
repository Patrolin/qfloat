#pragma once
#include "definitions.h"

#define optimizer_read_write_fence(value) asm volatile("" : "+x"(value))
#define optimizer_read_fence(value) asm volatile("" : "x"(value))
// TODO: timings
