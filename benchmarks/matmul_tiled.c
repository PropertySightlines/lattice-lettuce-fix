// matmul_tiled.c - Fair comparison with Salt's affine-loop-tile optimization
// Uses manual 32x32 cache-blocking (same tile size as Salt's MLIR pass)

#include <stdio.h>
#include <stdlib.h>

#define TILE_SIZE 32

void matmul_tiled(int n, double *restrict a, double *restrict b,
                  double *restrict c) {
  // Initialize c to zero (matches Salt's Tensor initialization)
  for (int i = 0; i < n * n; i++) {
    c[i] = 0.0;
  }

  // Tiled matrix multiplication with 32x32 blocks
  for (int ii = 0; ii < n; ii += TILE_SIZE) {
    for (int jj = 0; jj < n; jj += TILE_SIZE) {
      for (int kk = 0; kk < n; kk += TILE_SIZE) {
        // Process tile
        int i_max = (ii + TILE_SIZE < n) ? ii + TILE_SIZE : n;
        int j_max = (jj + TILE_SIZE < n) ? jj + TILE_SIZE : n;
        int k_max = (kk + TILE_SIZE < n) ? kk + TILE_SIZE : n;

        for (int i = ii; i < i_max; i++) {
          for (int j = jj; j < j_max; j++) {
            double sum = c[i * n + j];
            for (int k = kk; k < k_max; k++) {
              sum += a[i * n + k] * b[k * n + j];
            }
            c[i * n + j] = sum;
          }
        }
      }
    }
  }
}

int main(int argc, char **argv) {
  int n = 1024;
  double *a = (double *)malloc(n * n * sizeof(double));
  double *b = (double *)malloc(n * n * sizeof(double));
  double *c = (double *)malloc(n * n * sizeof(double));

  for (int i = 0; i < n * n; i++) {
    a[i] = 1.0 * argc;
    b[i] = 2.0;
  }

  matmul_tiled(n, a, b, c);

  // Checksum to prevent DCE
  double checksum = 0.0;
  for (int i = 0; i < n * n; i++) {
    checksum += c[i];
  }

  if (checksum != (2147483648.0 * argc)) {
    return 1;
  }

  free(a);
  free(b);
  free(c);
  return 0;
}
