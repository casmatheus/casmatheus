#include "core/math.h"

// FNV-1a hash
u64 hash(const char* str) {
  u64 hash = 0xcbf29ce484222325;
  while (*str) {
    hash ^= (unsigned char)*str++;
    hash *= 0x100000001b3;
  }
  return hash;
}
