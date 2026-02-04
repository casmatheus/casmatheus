#pragma once

#include "core/utility.h"
#include "core/primitive.h"
#include "debug/log.h"
#include "debug/trap.h"

template <typename T, typename E>
struct Result {
  bool success;
  T value;
  E error;

  static Result<T, E> Ok(T value) {
    Result<T, E> res;
    res.success = true;
    res.value = value;
    return res;
  }

  static Result<T, E> Err(E error) {
    Result<T, E> res;
    res.success = false;
    res.error = error;
    return res;
  }

  explicit operator bool() const { 
		return success; 
	}

  T unwrap() {
    if (!success) {
      logError("Result Unwrap Failed! Error present.");
			TRAP();
    }
    return move(value);
  }

  E unwrapErr() {
    if (success) {
    	logError("Result UnwrapErr Failed! Value present.");
			TRAP();
    }
    return move(error);
  }
};

enum class ErrorCode: u8 {
	OK = 0,
	FAILURE = 1,
	INVALID_ARGUMENT,
	NOT_INITIALIZED,
	PLATFORM_ERROR,
	UNSUPPORTED
};

