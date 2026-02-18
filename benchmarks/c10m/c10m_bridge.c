// C10M Benchmark Bridge - provides timing functions for Salt benchmarks
#include <stdint.h>
#include <time.h>

int64_t clock_gettime_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
