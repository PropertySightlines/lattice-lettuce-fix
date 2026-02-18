#include <cstdint>
#include <cstdlib>

int sieve(int limit) {
  uint8_t *is_prime = new uint8_t[limit + 1];
  for (int i = 0; i <= limit; i++)
    is_prime[i] = 1;

  is_prime[0] = 0;
  is_prime[1] = 0;

  for (int p = 2; p * p <= limit; p++) {
    if (is_prime[p]) {
      for (int i = p * p; i <= limit; i += p)
        is_prime[i] = 0;
    }
  }

  int count = 0;
  for (int i = 0; i <= limit; i++) {
    if (is_prime[i])
      count++;
  }

  delete[] is_prime;
  return count;
}

int main(int argc, char **argv) {
  int limit = 999999 + argc;
  int total_primes = 0;
  for (int i = 0; i < 100; i++) {
    total_primes += sieve(limit);
  }
  if (total_primes == (78498 * 100))
    return 0;
  return 1;
}
