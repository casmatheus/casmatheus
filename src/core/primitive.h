#pragma once

#include "compiler/context.h"

//clang-format off

// Standard Types
using i8  = signed char;
using i16 = signed short;
using i32 = signed int;
using i64 = signed long long;

using u8  = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

// Some Places define a special 32 bit boolean
// This Type is for compability with them
using bool32 = i32;

using bool8 = i8;

typedef float f32;
typedef double f64;

// Rad = Direction * pi/4
enum class Direction : u8 {
  East  			= 0,
	NorthEast   = 1,
  North       = 2,
  NorthWest   = 3,
  West        = 4,
  SouthWest   = 5,
  South       = 6,
  SouthEast   = 7	
};

// Numeric Limit Definitions
constexpr i8  i8_MAX  = ((i8)127);
constexpr i8  i8_MIN  = ((i8) - i8_MAX - 1);

constexpr i16 i16_MAX = ((i16)32767);
constexpr i16 i16_MIN = ((i16) - i16_MAX - 1);

constexpr i32 i32_MAX = ((i32)2147483647);
constexpr i32 i32_MIN = ((i32) - i32_MAX - 1);

constexpr i64 i64_MAX = ((i64)9223372036854775807ll);
constexpr i64 i64_MIN = ((i64) - i64_MAX - 1);

constexpr u8  u8_MAX  = ((u8)255u);
constexpr u8  u8_MIN  = ((u8)0u);

constexpr u16 u16_MAX = ((u16)65535u);
constexpr u16 u16_MIN = ((u16)0u);

constexpr u32 u32_MAX = ((u32)4294967295u);
constexpr u32 u32_MIN = ((u32)0u);

constexpr u64 u64_MAX = ((u64)18446744073709551615llu);
constexpr u64 u64_MIN = ((u64)0llu);

#ifdef COMPILER_CLANG
  constexpr f32 f32_INFINITY = (__builtin_huge_valf());
#else
  constexpr f32 f32_INFINITY = ((f32)(1e+300 * 1e+300));
#endif

// Extensions Rarely Used
#ifdef ARCH_64BIT
  typedef i64 iptr;
  typedef u64 uptr;
  constexpr u64 USIZE_MAX = u64_MAX;
  constexpr i64 ISIZE_MAX = i64_MAX;
#elif defined(ARCH_32BIT)
  typedef i32 iptr;
  typedef u32 uptr;
  constexpr u32 USIZE_MAX = u32_MAX;
  constexpr i32 ISIZE_MAX = i32_MAX;
#else
  #error "Platform Is Not x32 or x64"
#endif

typedef uptr usize;
typedef iptr isize;

// Static Asserts For Type Sizes
static_assert(sizeof(i8) == 1, "sizeof(i8) Must be 1 Byte");
static_assert(sizeof(i16) == 2, "sizeof(i16) Must be 2 Bytes");
static_assert(sizeof(i32) == 4, "sizeof(i32) Must be 4 Bytes");
static_assert(sizeof(i64) == 8, "sizeof(i64) Must be 8 Bytes");

static_assert(sizeof(u8) == 1, "sizeof(u8) Must be 1 Byte");
static_assert(sizeof(u16) == 2, "sizeof(u16) Must be 2 Bytes");
static_assert(sizeof(u32) == 4, "sizeof(u32) Must be 4 Bytes");
static_assert(sizeof(u64) == 8, "sizeof(u64) Must be 8 Bytes");

static_assert(sizeof(f32) == 4, "sizeof(f32) Must be 4 Bytes");
static_assert(sizeof(f64) == 8, "sizeof(f64) Must be 8 Bytes");

//clang-format on
