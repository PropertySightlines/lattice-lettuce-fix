#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

int sieve(int limit) {
  long size = (long)limit + 1;
  uint8_t *is_prime = (uint8_t *)malloc(size);
  if (!is_prime)
    return 0;

  // Init
  for (int i = 0; i <= limit; i++) {
    is_prime[i] = 1;
  }
  is_prime[0] = 0;
  is_prime[1] = 0;

  // Sieve
  for (long p = 2; p <= limit; p++) {
    if ((long)p * p > limit)
      break;
    if (is_prime[p]) {
      long j = p * p;
      while (j <= limit) {
        is_prime[j] = 0;
        j += p;
      }
    }
  }

  // Count
  int count = 0;
  for (int k = 0; k <= limit; k++) {
    if (is_prime[k]) {
      count++;
    }
  }

  free(is_prime);
  return count;
}

int main() {
  long total_primes = 0;
  for (int k = 0; k < 200; k++) {
    total_primes += sieve(1000000);
  }

  if (total_primes != 15699600) {
    printf("Mismatch: %ld\n", total_primes);
    return 1;
  }
  return 0;
}
