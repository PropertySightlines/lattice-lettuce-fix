#include <stdio.h>

int fib(int n) {
  if (n <= 1) {
    return n;
  }
  return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv) {
  int n = 39 + argc; // Should be 40 if 1 arg
  int res = fib(n);
  if (res == 102334155) {
    return 0;
  }
  return 1;
}
