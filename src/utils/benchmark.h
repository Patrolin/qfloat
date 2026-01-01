#pragma once
#include "definitions.h"

#define optimizer_fence(value) asm volatile("" : "+X"(value))
// TODO: timings
