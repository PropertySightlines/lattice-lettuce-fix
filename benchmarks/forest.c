#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct Node {
  struct Node *left;
  struct Node *right;
  int val;
} Node;

Node *make_tree(int depth) {
  if (depth == 0) {
    return NULL;
  }
  Node *n = (Node *)malloc(sizeof(Node));
  n->val = depth;
  n->left = make_tree(depth - 1);
  n->right = make_tree(depth - 1);
  return n;
}

void free_tree(Node *n) {
  if (!n) {
    return;
  }
  free_tree(n->left);
  free_tree(n->right);
  free(n);
}

int main() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long t0 = ts.tv_sec * 1000000000LL + ts.tv_nsec;

  // Depth 22 -> ~4M nodes (2^22 - 1)
  Node *root = make_tree(22);

  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long t1 = ts.tv_sec * 1000000000LL + ts.tv_nsec;

  printf("Build Time: %lld ns\n", t1 - t0);

  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long t2 = ts.tv_sec * 1000000000LL + ts.tv_nsec;

  free_tree(root);

  clock_gettime(CLOCK_MONOTONIC, &ts);
  long long t3 = ts.tv_sec * 1000000000LL + ts.tv_nsec;

  printf("Free Time: %lld ns\n", t3 - t2);
  printf("Total Churn: %lld ns\n", t3 - t0);

  return 0;
}
