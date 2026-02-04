#pragma once

// x64 or x32 ?
#if defined(__EMSCRIPTEN__)
  #define ARCH_WASM
  #define ARCH_LITTLE_ENDIAN 1
  #if defined(__wasm64__)
    #define ARCH_64BIT
  #else
    #define ARCH_32BIT
  #endif

#elif defined(__x86_64) || defined(__x86_64__) || defined(__amd64__) || defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
  #define ARCH_X64
  #define ARCH_LITTLE_ENDIAN 1
  #define ARCH_64BIT

#elif defined(i386) || defined(__i386) || defined(__i386__) || defined(_M_IX86)
  #define ARCH_X32
  #define ARCH_LITTLE_ENDIAN 1
  #define ARCH_32BIT
#else
  #error "Architecture Not Supported"
#endif

// Compiler
#ifdef __clang__
  #define COMPILER_CLANG
#elif defined(_MSC_EXTENSIONS)
#define COMPILER_CL
#elif defined(__GNUC__) || defined(__GNUG__)
  #define COMPILER_GCC
#else
	#error "Compiler Not Supported"
#endif

// Operating System
#ifdef __EMSCRIPTEN__
  #define OS_BROWSER
#elif defined(_WIN32)
  #define OS_WINDOWS
#else
  #error "Operating System Not Supported"
#endif






