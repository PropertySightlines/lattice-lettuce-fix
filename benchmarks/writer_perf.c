// Writer Protocol Performance Benchmark - C baseline
// Measures direct-to-buffer formatting using byte-by-byte loops
// Matches Salt's current loop-based write_str implementation for fair
// comparison

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATIONS 10000000
#define BUFFER_SIZE 4096

// Simulate a "Writer" that accumulates to a buffer
typedef struct {
  char *data;
  size_t len;
  size_t capacity;
} Buffer;

// Byte-by-byte push (matches Salt's push_byte approach)
static inline void buffer_push_byte(Buffer *b, char c) {
  if (b->len < b->capacity) {
    b->data[b->len++] = c;
  }
}

// Loop-based write_str (matches Salt's current approach)
static inline void buffer_write_str(Buffer *b, const char *s, size_t slen) {
  for (size_t i = 0; i < slen; i++) {
    buffer_push_byte(b, s[i]);
  }
}

// Inlined integer formatting (digit extraction + byte-by-byte push)
static inline void buffer_write_i32(Buffer *b, int val) {
  char tmp[16];
  int len = 0;
  int n = val;
  int neg = n < 0;
  if (neg)
    n = -n;

  if (n == 0) {
    buffer_push_byte(b, '0');
    return;
  }

  while (n > 0) {
    tmp[len++] = '0' + (n % 10);
    n /= 10;
  }

  if (neg)
    buffer_push_byte(b, '-');

  for (int i = len - 1; i >= 0; i--) {
    buffer_push_byte(b, tmp[i]);
  }
}

// Inlined i64 formatting (matches Salt's write_i64)
static inline void buffer_write_i64(Buffer *b, long val) {
  char tmp[24];
  int len = 0;
  long n = val;
  int neg = n < 0;
  if (neg)
    n = -n;

  if (n == 0) {
    buffer_push_byte(b, '0');
    return;
  }

  while (n > 0) {
    tmp[len++] = '0' + (n % 10);
    n /= 10;
  }

  if (neg)
    buffer_push_byte(b, '-');

  for (int i = len - 1; i >= 0; i--) {
    buffer_push_byte(b, tmp[i]);
  }
}

int main(int argc, char **argv) {
  char storage[BUFFER_SIZE];
  Buffer buf = {.data = storage, .len = 0, .capacity = BUFFER_SIZE};

  long total_len = 0;

  for (int i = 0; i < ITERATIONS; i++) {
    // Reset buffer each iteration
    buf.len = 0;

    // Direct write calls using byte-by-byte approach
    buffer_write_str(&buf, "Item ", 5);
    buffer_write_i32(&buf, i);
    buffer_write_str(&buf, ": val = ", 8);
    buffer_write_i64(&buf, (long)i * 1000);

    total_len += buf.len;
  }

  // Prevent DCE
  if (total_len == 0)
    return 1;
  return 0;
}
