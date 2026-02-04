#pragma once

#include "compiler/context.h"

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
  #define TRAP() __builtin_trap()
#elif defined(COMPILER_CL)
  #define TRAP() __debugbreak()
#else
  // Fallback: Dereference null pointer to force a crash
  #define TRAP() (*((volatile int*)0) = 0)
#endif
