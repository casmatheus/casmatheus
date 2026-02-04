#include "os/time.h"
#include "debug/log.h"

#include <emscripten.h>

Instant Instant::Now() {
  f64 now_ms = emscripten_get_now();
	return Instant { (u64)(now_ms * 1'000'000.0) };
}
