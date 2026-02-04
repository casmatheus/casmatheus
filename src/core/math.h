#pragma once

#include "core/primitive.h"

template <typename T>
inline T Abs(T value) {
  return (value < 0) ? -value : value;
}

inline f32 Abs(f32 value) {
	return (value < 0.0f) ? -value : value; 
}

u64 hash(const char* str);
