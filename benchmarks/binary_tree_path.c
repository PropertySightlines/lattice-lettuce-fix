
#include <stdio.h>
#include <stdlib.h>

typedef struct TreeNode {
  int val;
  struct TreeNode *left;
  struct TreeNode *right;
} TreeNode;

TreeNode *create_node(int val) {
  TreeNode *node = (TreeNode *)malloc(sizeof(TreeNode));
  node->val = val;
  node->left = NULL;
  node->right = NULL;
  return node;
}

TreeNode *build_tree(int depth) {
  if (depth == 0)
    return NULL;
  TreeNode *node = create_node(depth);
  node->left = build_tree(depth - 1);
  node->right = build_tree(depth - 1);
  return node;
}

void free_tree(TreeNode *root) {
  if (!root)
    return;
  free_tree(root->left);
  free_tree(root->right);
  free(root);
}

int dfs(TreeNode *root, int sum) {
  if (!root)
    return 0;

  int current = sum * 10 + root->val;

  if (!root->left && !root->right) {
    return current;
  }

  return dfs(root->left, current) + dfs(root->right, current);
}

int main() {
  TreeNode *root = build_tree(12);
  int total = 0;

  for (int i = 0; i < 600; i++) {
    total += dfs(root, 0);
  }

  free_tree(root);
  printf("Total: %d\n", total);
  return 0;
}
