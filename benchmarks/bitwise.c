#include <stdio.h>

int main(int argc, char **argv) {
  int iterations = 100000000;
  unsigned long long total = 0;
  unsigned long long base = (unsigned long long)argc | 0x123456789ABCDEF0ULL;

  for (int i = 0; i < iterations; i++) {
    // Use compiler builtin to match Salt's llvm.ctpop intrinsic
    total += __builtin_popcountll(base ^ (unsigned long long)i);
  }

  if (total > 0) {
    return 0;
  }
  return 1;
}
