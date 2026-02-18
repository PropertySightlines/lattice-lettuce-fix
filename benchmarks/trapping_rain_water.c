
#include <stdio.h>
#include <stdlib.h>

int trap(int *height, int heightSize) {
  if (heightSize <= 2)
    return 0;

  int *left = (int *)malloc(heightSize * sizeof(int));
  int *right = (int *)malloc(heightSize * sizeof(int));

  left[0] = height[0];
  for (int i = 1; i < heightSize; i++) {
    int h = height[i];
    int prev = left[i - 1];
    left[i] = (h > prev) ? h : prev;
  }

  right[heightSize - 1] = height[heightSize - 1];
  for (int i = heightSize - 2; i >= 0; i--) {
    int h = height[i];
    int next = right[i + 1];
    right[i] = (h > next) ? h : next;
  }

  int ans = 0;
  for (int i = 0; i < heightSize; i++) {
    int l = left[i];
    int r = right[i];
    int min_h = (l < r) ? l : r;
    ans += min_h - height[i];
  }

  free(left);
  free(right);
  return ans;
}

int main() {
  int size = 100000;
  int *height = (int *)malloc(size * sizeof(int));

  // Generate zigzag pattern
  for (int i = 0; i < size; i++) {
    height[i] = i % 10;
    if ((i / 10) % 2 == 1) {
      height[i] = 9 - (i % 10);
    }
  }

  int total = 0;
  for (int k = 0; k < 1000; k++) {
    total += trap(height, size);
  }

  free(height);
  printf("Total: %d\n", total);
  return 0;
}
