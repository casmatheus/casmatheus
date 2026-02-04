#pragma once

#include "core/primitive.h"
#include "core/utility.h"
#include <stdlib.h>
#include <new>

template <typename T>
struct Vec {
  T* 		data 		 = nullptr;
  usize size     = 0;
  usize capacity = 0;

  Vec() = default;

  ~Vec() {
    clear();
		free(data);
  }

  inline bool empty() const { return size == 0; }
  
  void push(const T& value) {
    if (size == capacity) { 
			grow();
		}
    new (data + size) T(value);
    size++;
  }

  void push(T&& value) {
    if (size == capacity) { 
			grow();
		}
    new (data + size) T(move(value));
    size++;
  }

  T& operator[](usize index) { return data[index]; }
  const T& operator[](usize index) const { return data[index]; }

  void clear() {
    for (usize i = 0; i < size; ++i) {
      data[i].~T();
    }
    size = 0;
  }

  void reserve(usize new_cap) {
    if (new_cap > capacity) { 
			reallocate(new_cap);
		}
  }

  T* begin() { return data; }
  T* end()   { return data + size; }
  const T* begin() const { return data; }
  const T* end() const   { return data + size; }

  Vec(Vec&& other) {
    data = other.data;
    size = other.size;
    capacity = other.capacity;
    
    other.data = nullptr;
    other.size = 0;
    other.capacity = 0;
  }

private:
  void grow() {
    usize new_cap = (capacity == 0) ? 8 : capacity * 2;
    reallocate(new_cap);
  }

  void reallocate(usize new_cap) {
    T* new_block = (T*)malloc(new_cap * sizeof(T));
    
    for (usize i = 0; i < size; ++i) {
      new (new_block + i) T(move(data[i]));
      data[i].~T();
    }

		free(data);
    
    data = new_block;
    capacity = new_cap;
  }
  
  Vec(const Vec&) = delete;
  Vec& operator=(const Vec&) = delete;
};
