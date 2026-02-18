#include <stdio.h>

typedef struct {
  int data;
} Vec_int;

Vec_int add_generic(Vec_int a, Vec_int b) {
  Vec_int res = {a.data + b.data};
  return res;
}

int main(int argc, char **argv) {
  int sum = 0;

  // Loop-carried dependency: each iteration depends on previous sum
  for (int i = 0; i < 100000000; ++i) {
    // Use sum from previous iteration to create dependency
    int val1 = (i ^ (i >> 3)) + argc + (sum & 0xFF);
    int val2 = ((i * 17) ^ (i >> 5)) + argc;

    Vec_int v1 = {val1};
    Vec_int v2 = {val2};
    Vec_int res = add_generic(v1, v2);
    sum += res.data;
  }

  // Prevent DCE
  if (sum != 0) {
    return 0;
  }
  return 1;
}
