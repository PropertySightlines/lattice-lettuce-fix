#include <stdio.h>

unsigned long long popcount(unsigned long long n) {
  unsigned long long count = 0;
  while (n > 0) {
    n &= (n - 1);
    count++;
  }
  return count;
}

int main(int argc, char **argv) {
  int iterations = 1000000;
  unsigned long long total = 0;
  unsigned long long base = (unsigned long long)argc | 0x123456789ABCDEF0ULL;

  for (int i = 0; i < iterations; i++) {
    total += popcount(base ^ (unsigned long long)i);
  }

  if (total > 0)
    return 0;
  return 1;
}
