
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
  int key;
  int val;
  struct Node *prev;
  struct Node *next;
} Node;

typedef struct LRUCache {
  int capacity;
  int count;
  Node *head;
  Node *tail;
  Node *map[10000];
} LRUCache;

Node *node_create(int key, int val) {
  Node *node = (Node *)malloc(sizeof(Node));
  node->key = key;
  node->val = val;
  node->prev = NULL;
  node->next = NULL;
  return node;
}

LRUCache *cache_create(int capacity) {
  LRUCache *cache = (LRUCache *)malloc(sizeof(LRUCache));
  cache->capacity = capacity;
  cache->count = 0;
  cache->head = node_create(0, 0);
  cache->tail = node_create(0, 0);
  cache->head->next = cache->tail;
  cache->tail->prev = cache->head;

  for (int i = 0; i < 10000; i++) {
    cache->map[i] = NULL;
  }
  return cache;
}

void remove_node(Node *node) {
  node->prev->next = node->next;
  node->next->prev = node->prev;
}

void add_to_head(LRUCache *cache, Node *node) {
  node->prev = cache->head;
  node->next = cache->head->next;
  cache->head->next->prev = node;
  cache->head->next = node;
}

int get(LRUCache *cache, int key) {
  Node *node = cache->map[key];
  if (node) {
    remove_node(node);
    add_to_head(cache, node);
    return node->val;
  }
  return -1;
}

void put(LRUCache *cache, int key, int val) {
  Node *node = cache->map[key];
  if (node) {
    node->val = val;
    remove_node(node);
    add_to_head(cache, node);
  } else {
    Node *new_node = node_create(key, val);
    cache->map[key] = new_node;
    add_to_head(cache, new_node);
    cache->count++;
    if (cache->count > cache->capacity) {
      Node *lru = cache->tail->prev;
      remove_node(lru);
      cache->map[lru->key] = NULL;
      cache->count--;
      // free(lru); Leaked to match Salt
    }
  }
}

int main() {
  int checksum = 0;
  for (int k = 0; k < 1000; k++) {
    LRUCache *cache = cache_create(100);
    for (int i = 0; i < 1000; i++) {
      put(cache, i, i * 10);
      if (i % 2 == 0) {
        checksum += get(cache, i / 2);
      }
    }
  }
  printf("Checksum: %d\n", checksum);
  return 0;
}
