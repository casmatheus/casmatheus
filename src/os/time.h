#pragma once

#include "core/primitive.h"

struct Duration {
  u64 ns;

  Duration() : ns(0) {}
  explicit Duration(u64 ns) : ns(ns) {}

  static Duration FromNanoseconds(u64 ns) { 
		return Duration(ns); 
	}

  static Duration FromSeconds(f32 s) { 
		return Duration((u64)(s * 1e9f)); 
	}

  static Duration FromMilliseconds(u64 ms) { 
		return Duration(ms * 1'000'000); 
	}

  f32 asSeconds() const { 
    return (f32)ns * 1e-9f; 
  }
  
  Duration operator+(const Duration& other) const { 
		return Duration(ns + other.ns); 
	}
  Duration operator-(const Duration& other) const { 
		return Duration(ns - other.ns); 
	}
  
	bool operator==(const Duration& other) const { 
		return ns == other.ns; 
	}
  bool operator!=(const Duration& other) const { 
		return ns != other.ns; 
	}
  bool operator< (const Duration& other) const { 
		return ns < other.ns; 
	}
  bool operator> (const Duration& other) const { 
		return ns > other.ns; 
	}
  bool operator<=(const Duration& other) const { 
		return ns <= other.ns; 
	}
  bool operator>=(const Duration& other) const { 
		return ns >= other.ns; 
	}
};

struct Instant {
  u64 ns; // Time since Epoch 

  static Instant Now();

	Duration elapsed() const {
    return Instant::Now() - *this;
  }

  Duration operator-(const Instant& other) const {
    return Duration(ns - other.ns);
  }

  Instant operator+(const Duration& other) const {
    return Instant{ ns + other.ns };
  }

  Instant operator-(const Duration& other) const {
    return Instant{ ns - other.ns };
  }

  Instant& operator+=(const Duration& other) {
    ns += other.ns;
    return *this;
  }

  bool operator==(const Instant& other) const { 
		return ns == other.ns; 
	}
  bool operator!=(const Instant& other) const { 
		return ns != other.ns; 
	}
  bool operator< (const Instant& other) const { 
		return ns < other.ns; 
	}
  bool operator> (const Instant& other) const { 
		return ns > other.ns; 
	}
  bool operator<=(const Instant& other) const { 
		return ns <= other.ns; 
	}
};
