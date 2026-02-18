#include <stdint.h>
#include <stdlib.h>

typedef struct {
  int r, g, b;
} Pixel;

int main(int argc, char **argv) {
  // Allocate real memory
  int N = 10000000;
  Pixel *pixels = (Pixel *)malloc(N * sizeof(Pixel));

  // Initialize with dynamic values
  for (int i = 0; i < N; i++) {
    pixels[i].r = i + argc;
    pixels[i].g = i * 2;
    pixels[i].b = i ^ argc;
  }

  // Real work: data-dependent traversal
  long sum = 0;
  int idx = argc & (N - 1);
  for (long i = 0; i < 100000000; ++i) {
    Pixel *p = &pixels[idx];
    sum += p->r + p->g + p->b;
    idx = (idx + p->r) & (N - 1); // Data-dependent next index
  }

  free(pixels);

  // Prevent DCE
  if (sum != 0) {
    return 0;
  }
  return 1;
}
