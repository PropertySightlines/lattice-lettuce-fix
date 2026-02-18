#include <stdint.h>
#include <stdio.h>

void printf_shim(const char *fmt, int64_t val) { printf(fmt, val); }

void salt_yield_check(void *region) {
  // No-op for benchmarking
}

extern void salt_main(void *region);

#include <stdlib.h>

void *malloc_shim(size_t size) { return malloc(size); }

int main() {
  uint8_t dummy_region[1024];
  salt_main(dummy_region);
  return 0;
}
