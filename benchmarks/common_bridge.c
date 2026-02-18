#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Salt Status Codes (ULSR)
#define SALT_STATUS_OK 0
#define SALT_STATUS_PENDING 100
#define SALT_STATUS_FOUND 200
#define SALT_STATUS_NOT_FOUND 404
#define SALT_STATUS_INTERNAL_ERROR 500

__attribute__((weak)) void printf_shim(const char *fmt, int64_t val) {
  printf(fmt, val);
}

__attribute__((weak)) void *malloc_shim(size_t size) { return malloc(size); }

// Default Heartbeat Handler (Weak)
// If the Salt program defines its own salt_yield_check (like gauntlet_p99),
// that one should take precedence if the linker supports it,
// or we just avoid linking this if the Salt program provides it.
// For now, let's just make it a standard function and we'll handle
// the gauntlet specifically.
__attribute__((weak)) void salt_yield_check(void *region) {
  // Default: No-op
}

__attribute__((weak)) void sys_yield() {
  // Default: No-op
}

struct window_t {
  void *ptr;
  uint64_t len;
};

struct window_t map_window(int32_t phys, int32_t size, int32_t r) {
  struct window_t w = {(void *)(uintptr_t)phys, (uint64_t)size};
  return w;
}

__attribute__((weak)) uint64_t rdtsc() {
#if defined(__x86_64__)
  uint32_t lo, hi;
  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
#elif defined(__aarch64__)
  uint64_t val;
  __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
  return val;
#else
  return 0;
#endif
}

__attribute__((weak)) int64_t bench_clock() {
  return (int64_t)rdtsc(); // simple fallback
}

__attribute__((weak)) uint32_t sys_get_pid() { return 1; }

__attribute__((weak)) uint32_t sys_spawn(uint64_t entry) { return 0; }
__attribute__((weak)) uint32_t sys_send(uint32_t target, uint64_t page) {
  return 0;
}
__attribute__((weak)) uint32_t sys_receive(uint64_t page) { return 0; }
