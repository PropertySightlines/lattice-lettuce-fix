#define _XOPEN_SOURCE 600
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Mock alloc_page that actually allocates a large 64MB chunk
// so our simple region allocator doesn't OOM.
void *alloc_page() {
  void *ptr = malloc(64 * 1024 * 1024);
  if (!ptr) {
    printf("Failed to allocate 64MB region backing\n");
    exit(1);
  }
  memset(ptr, 0, 64 * 1024 * 1024);
  return ptr;
}

int64_t bench_clock() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)(ts.tv_sec * 1000 * 1000 * 1000 + ts.tv_nsec);
}

// printf_shim removed - using native printf from Salt

// Stub for potential other externs
void prevent_dce(void *p) {}

// NOTE: Salt's main() is the entry point now
