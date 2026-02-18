// F-String Performance Benchmark - C baseline
// Measures string formatting performance with sprintf-style formatting
// Comparable to Salt f-strings and Rust format!

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATIONS 10000000

int main(int argc, char **argv) {
  char buffer[64];
  long total_len = 0;
  double val = 3.14159;

  for (int i = 0; i < ITERATIONS; i++) {
    // Format string with integer and float
    int len =
        sprintf(buffer, "Item %d: value = %.2f", i, val + (double)i * 0.001);
    total_len += len;
  }

  // Prevent DCE - use the result
  if (total_len == 0) {
    return 1;
  }
  return 0;
}
