// longest_consecutive.c - LeetCode 128: Longest Consecutive Sequence
// O(n) solution using HashSet (open-addressing)

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INITIAL_CAP 8
#define LOAD_FACTOR_NUM 7
#define LOAD_FACTOR_DEN 8
#define CTRL_EMPTY -128

typedef struct {
  int8_t *ctrl;
  int64_t *keys;
  int64_t len;
  int64_t cap;
  int64_t growth_left;
} HashSet;

static inline uint64_t hash_i64(int64_t val) {
  uint64_t h = (uint64_t)val ^ 0xa0761d6478bd642fULL;
  h = (h ^ (h >> 32)) * 0xe7037ed1a0b428dbULL;
  return h ^ (h >> 32);
}

HashSet *hashset_new(int64_t min_cap) {
  HashSet *set = (HashSet *)malloc(sizeof(HashSet));
  int64_t cap = INITIAL_CAP;
  while (cap < min_cap)
    cap *= 2;

  set->ctrl = (int8_t *)malloc(cap);
  memset(set->ctrl, CTRL_EMPTY, cap);
  set->keys = (int64_t *)malloc(cap * sizeof(int64_t));
  set->len = 0;
  set->cap = cap;
  set->growth_left = (cap * LOAD_FACTOR_NUM) / LOAD_FACTOR_DEN;
  return set;
}

void hashset_grow(HashSet *set) {
  int64_t old_cap = set->cap;
  int8_t *old_ctrl = set->ctrl;
  int64_t *old_keys = set->keys;

  int64_t new_cap = old_cap * 2;
  set->ctrl = (int8_t *)malloc(new_cap);
  memset(set->ctrl, CTRL_EMPTY, new_cap);
  set->keys = (int64_t *)malloc(new_cap * sizeof(int64_t));
  set->len = 0;
  set->cap = new_cap;
  set->growth_left = (new_cap * LOAD_FACTOR_NUM) / LOAD_FACTOR_DEN;

  for (int64_t i = 0; i < old_cap; i++) {
    if (old_ctrl[i] >= 0) {
      int64_t key = old_keys[i];
      uint64_t hash = hash_i64(key);
      int8_t tag = (int8_t)(hash & 0x7F);
      int64_t idx = (hash >> 7) % new_cap;
      while (set->ctrl[idx] != CTRL_EMPTY)
        idx = (idx + 1) % new_cap;
      set->ctrl[idx] = tag;
      set->keys[idx] = key;
      set->len++;
      set->growth_left--;
    }
  }
  free(old_ctrl);
  free(old_keys);
}

void hashset_insert(HashSet *set, int64_t key) {
  if (set->growth_left == 0)
    hashset_grow(set);

  uint64_t hash = hash_i64(key);
  int8_t tag = (int8_t)(hash & 0x7F);
  int64_t idx = (hash >> 7) % set->cap;

  while (set->ctrl[idx] != CTRL_EMPTY) {
    if (set->ctrl[idx] == tag && set->keys[idx] == key)
      return; // Already exists
    idx = (idx + 1) % set->cap;
  }

  set->ctrl[idx] = tag;
  set->keys[idx] = key;
  set->len++;
  set->growth_left--;
}

int hashset_contains(HashSet *set, int64_t key) {
  if (set->cap == 0)
    return 0;

  uint64_t hash = hash_i64(key);
  int8_t tag = (int8_t)(hash & 0x7F);
  int64_t idx = (hash >> 7) % set->cap;
  int64_t probes = 0;

  while (probes < set->cap) {
    if (set->ctrl[idx] == CTRL_EMPTY)
      return 0;
    if (set->ctrl[idx] == tag && set->keys[idx] == key)
      return 1;
    idx = (idx + 1) % set->cap;
    probes++;
  }
  return 0;
}

void hashset_free(HashSet *set) {
  free(set->ctrl);
  free(set->keys);
  free(set);
}

// LeetCode 128: Longest Consecutive Sequence - O(n) solution
int64_t longest_consecutive(int64_t *nums, int64_t n) {
  if (n == 0)
    return 0;

  HashSet *set = hashset_new(n);
  for (int64_t i = 0; i < n; i++) {
    hashset_insert(set, nums[i]);
  }

  int64_t best = 0;
  for (int64_t i = 0; i < n; i++) {
    int64_t num = nums[i];
    // Only start counting if this is the start of a sequence
    if (!hashset_contains(set, num - 1)) {
      int64_t len = 1;
      while (hashset_contains(set, num + len))
        len++;
      if (len > best)
        best = len;
    }
  }

  hashset_free(set);
  return best;
}

int main() {
  // Benchmark: Multiple rounds with large random arrays
  int64_t total = 0;
  int64_t n = 100000;
  int64_t *nums = (int64_t *)malloc(n * sizeof(int64_t));

  for (int round = 0; round < 100; round++) {
    // Generate pseudo-random array with some consecutive sequences
    for (int64_t i = 0; i < n; i++) {
      nums[i] = (i * 1103515245 + 12345 + round) % (n * 10);
    }
    total += longest_consecutive(nums, n);
  }

  printf("Total: %lld\n", (long long)total);
  free(nums);
  return 0;
}
