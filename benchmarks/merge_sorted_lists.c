
#include <stdio.h>
#include <stdlib.h>

struct ListNode {
  int val;
  struct ListNode *next;
};

struct ListNode *create_list(int len, int start, int step) {
  if (len <= 0)
    return NULL;
  struct ListNode *node = (struct ListNode *)malloc(sizeof(struct ListNode));
  node->val = start;
  node->next = create_list(len - 1, start + step, step);
  return node;
}

void free_list(struct ListNode *head) {
  if (!head)
    return;
  free_list(head->next);
  free(head);
}

struct ListNode *merge_two_lists(struct ListNode *l1, struct ListNode *l2) {
  if (!l1)
    return l2;
  if (!l2)
    return l1;

  if (l1->val < l2->val) {
    l1->next = merge_two_lists(l1->next, l2);
    return l1;
  } else {
    l2->next = merge_two_lists(l1, l2->next);
    return l2;
  }
}

int main() {
  int checksum = 0;

  for (int i = 0; i < 5000; i++) {
    struct ListNode *l1 = create_list(100, 0, 2);
    struct ListNode *l2 = create_list(100, 1, 2);
    struct ListNode *merged = merge_two_lists(l1, l2);

    struct ListNode *curr = merged;
    while (curr) {
      checksum += curr->val;
      curr = curr->next;
    }

    free_list(merged);
  }

  printf("Checksum: %d\n", checksum);
  return 0;
}
