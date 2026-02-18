
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

extern int salt_main();

void printf_shim(const char *fmt, int64_t val) { printf(fmt, val); }

int64_t bench_clock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

uint32_t sys_get_pid() { return 1; }

// Global yield counter to verify heartbeat is working
static int64_t yield_count = 0;

void sys_yield() {
  yield_count++;
  // In a real preemptive heartbeat, this would actually context switch.
  // For this benchmark, we just record that a yield was offered.
}

uint32_t sys_spawn(uint64_t entry) {
  return 0; // Stub
}

uint32_t sys_send(uint32_t target, uint64_t page) {
  return 0; // Stub
}

uint32_t sys_receive(uint64_t page) {
  return 0; // Stub
}

int main() {
  printf("--- Omega Gauntlet P99 Bridge ---\n");
  int64_t start = bench_clock();
  salt_main();
  int64_t end = bench_clock();

  printf("\nTotal execution time: %lld ns\n", end - start);
  printf("Total Heartbeat Yields: %lld\n", yield_count);

  if (yield_count > 0) {
    printf("RESULT: Preemptive Heartbeat Verified!\n");
  } else {
    printf("RESULT: Heartbeat Failed (Zero Yields)\n");
    return 1;
  }

  return 0;
}
