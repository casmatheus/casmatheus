#pragma once

#include "os/time.h"
#include "debug/log.h"
#include "core/primitive.h"

#ifdef DEBUG
constexpr u32 MAX_SAMPLES = 120;

struct PerfStats {
  const char* name;
  f32 samples[MAX_SAMPLES];
  u32 cursor = 0;
  u32 count = 0;

  void Record(f32 ms) {
    samples[cursor] = ms;
    cursor = (cursor + 1) % MAX_SAMPLES;
    if (count < MAX_SAMPLES) count++;
  }

  void Print() const {
    if (count == 0) return;
    f32 min = 1e10f, max = 0.0f, avg = 0.0f;
    for (u32 i = 0; i < count; ++i) {
      if (samples[i] < min) min = samples[i];
      if (samples[i] > max) max = samples[i];
      avg += samples[i];
    }
    avg /= (f32)count;
    printf("%s | Avg: %.3fms | Max: %.3fms | Min: %.3fms\n", name, avg, max, min);
  }
};

struct PerfRegistry {
  static constexpr u32 MAX_COUNTERS = 16;
  PerfStats counters[MAX_COUNTERS];
  u32 counterCount = 0;

  static PerfRegistry& Get() {
    static PerfRegistry instance;
    return instance;
  }

  PerfStats* GetCounter(const char* name) {
    for (u32 i = 0; i < counterCount; ++i) {
      if (counters[i].name == name) return &counters[i];
    }
    if (counterCount < MAX_COUNTERS) {
      counters[counterCount].name = name;
      return &counters[counterCount++];
    }
    return nullptr;
  }

  void PrintAll() {
    printf("--- Performance Report ---\n");
    for (u32 i = 0; i < counterCount; ++i) counters[i].Print();
    printf("--------------------------\n");
  }
};

struct ScopedTimer {
  PerfStats* stats;
  Instant start;
  ScopedTimer(const char* name) {
    stats = PerfRegistry::Get().GetCounter(name);
    start = Instant::Now();
  }
  ~ScopedTimer() {
    if (stats) stats->Record(start.elapsed().asSeconds() * 1000.0f);
  }
};

#define PERF_SCOPE(name) ScopedTimer timer_##__LINE__(name)
#define PERF_REPORT() PerfRegistry::Get().PrintAll()
#else
#define PERF_SCOPE(name) ((void)0)
#define PERF_REPORT() ((void)0)
#endif


















