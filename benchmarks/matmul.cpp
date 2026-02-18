#include <stdio.h>
#include <stdlib.h>
#include <vector>

void matmul(int n, const std::vector<double> &a, const std::vector<double> &b,
            std::vector<double> &c) {
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      double sum = 0.0;
      for (int k = 0; k < n; k++) {
        sum += a[i * n + k] * b[k * n + j];
      }
      c[i * n + j] = sum;
    }
  }
}

int main(int argc, char **argv) {
  int n = 512;
  std::vector<double> a(n * n, 1.0 * argc);
  std::vector<double> b(n * n, 2.0);
  std::vector<double> c(n * n);

  matmul(n, a, b, c);

  double checksum = 0.0;
  for (double val : c) {
    checksum += val;
  }

  if (checksum != (268435456.0 * argc)) {
    return 1;
  }

  return 0;
}
