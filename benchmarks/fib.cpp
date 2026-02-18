#include <iostream>

int fib(int n) {
  if (n <= 1)
    return n;
  return fib(n - 1) + fib(n - 2);
}

int main(int argc, char **argv) {
  int n = 34 + argc;
  int res = fib(n);
  if (res == 9227465)
    return 0;
  return 1;
}
