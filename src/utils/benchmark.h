#pragma once
#include "definitions.h"

#define optimizer_fence(value) asm volatile("" : "+x"(value))
// TODO: timings
