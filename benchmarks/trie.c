
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TrieNode {
  struct TrieNode *children[26];
  bool is_word;
} TrieNode;

TrieNode *create_node() {
  TrieNode *node = (TrieNode *)malloc(sizeof(TrieNode));
  for (int i = 0; i < 26; i++) {
    node->children[i] = NULL;
  }
  node->is_word = false;
  return node;
}

void insert(TrieNode *root, const char *word) {
  TrieNode *curr = root;
  for (int i = 0; word[i] != '\0'; i++) {
    int idx = word[i] - 'a';
    if (curr->children[idx] == NULL) {
      curr->children[idx] = create_node();
    }
    curr = curr->children[idx];
  }
  curr->is_word = true;
}

bool search(TrieNode *root, const char *word) {
  TrieNode *curr = root;
  for (int i = 0; word[i] != '\0'; i++) {
    int idx = word[i] - 'a';
    if (curr->children[idx] == NULL) {
      return false;
    }
    curr = curr->children[idx];
  }
  return curr->is_word;
}

int main() {
  TrieNode *root = create_node();

  char word[6];
  word[5] = '\0';

  printf("Inserting 700k words...\n");
  for (int i = 0; i < 700000; i++) {
    word[0] = (i % 26) + 97;
    word[1] = ((i / 26) % 26) + 97;
    word[2] = ((i / 676) % 26) + 97;
    word[3] = ((i / 17576) % 26) + 97;
    word[4] = (i % 7) + 97;
    insert(root, word);
  }

  printf("Searching 700k words...\n");
  int found = 0;
  for (int i = 0; i < 700000; i++) {
    word[0] = (i % 26) + 97;
    word[1] = ((i / 26) % 26) + 97;
    word[2] = ((i / 676) % 26) + 97;
    word[3] = ((i / 17576) % 26) + 97;
    word[4] = (i % 7) + 97;
    if (search(root, word)) {
      found++;
    }
  }
  printf("Found: %d\n", found);

  return 0;
}
