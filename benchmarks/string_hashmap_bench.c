// string_hashmap_bench.c - String Key HashMap Benchmark
// FNV-1a hash for string keys

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 8
#define LOAD_FACTOR_NUM 7
#define LOAD_FACTOR_DEN 8
#define MAX_KEY_LEN 32

#define CTRL_EMPTY -128
#define CTRL_DELETED -2

typedef struct {
  char key[MAX_KEY_LEN];
  int64_t val;
} Entry;

typedef struct {
  int8_t *ctrl;
  Entry *entries;
  int64_t len;
  int64_t cap;
  int64_t growth_left;
} HashMap;

// FNV-1a hash for strings (matching Salt's hash_bytes)
static inline uint64_t hash_string(const char *s) {
  uint64_t h = 0xcbf29ce484222325ULL;
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 0x100000001b3ULL;
  }
  return h;
}

HashMap *hashmap_new(int64_t min_cap) {
  HashMap *map = (HashMap *)malloc(sizeof(HashMap));

  if (min_cap <= 0) {
    map->ctrl = NULL;
    map->entries = NULL;
    map->len = 0;
    map->cap = 0;
    map->growth_left = 0;
    return map;
  }

  int64_t cap = INITIAL_CAP;
  while (cap < min_cap)
    cap *= 2;

  map->ctrl = (int8_t *)malloc(cap);
  memset(map->ctrl, CTRL_EMPTY, cap);
  map->entries = (Entry *)malloc(cap * sizeof(Entry));
  map->len = 0;
  map->cap = cap;
  map->growth_left = (cap * LOAD_FACTOR_NUM) / LOAD_FACTOR_DEN;

  return map;
}

void hashmap_grow(HashMap *map);

int64_t hashmap_get(HashMap *map, const char *key) {
  if (map->cap == 0)
    return 0;

  uint64_t hash = hash_string(key);
  int8_t tag = (int8_t)(hash & 0x7F);
  int64_t idx = ((hash >> 7) % map->cap);
  int64_t probes = 0;

  while (probes < map->cap) {
    int8_t ctrl = map->ctrl[idx];

    if (ctrl == tag) {
      if (strcmp(map->entries[idx].key, key) == 0) {
        return map->entries[idx].val;
      }
    }

    if (ctrl == CTRL_EMPTY)
      return 0;

    idx = (idx + 1) % map->cap;
    probes++;
  }
  return 0;
}

void hashmap_insert(HashMap *map, const char *key, int64_t val) {
  if (map->growth_left == 0) {
    hashmap_grow(map);
  }

  uint64_t hash = hash_string(key);
  int8_t tag = (int8_t)(hash & 0x7F);
  int64_t idx = ((hash >> 7) % map->cap);
  int64_t first_deleted = -1;
  int64_t probes = 0;

  while (probes < map->cap) {
    int8_t ctrl = map->ctrl[idx];

    if (ctrl == tag) {
      if (strcmp(map->entries[idx].key, key) == 0) {
        map->entries[idx].val = val;
        return;
      }
    }

    if (ctrl == CTRL_DELETED && first_deleted == -1) {
      first_deleted = idx;
    }

    if (ctrl == CTRL_EMPTY) {
      int64_t insert_idx = (first_deleted >= 0) ? first_deleted : idx;
      map->ctrl[insert_idx] = tag;
      strncpy(map->entries[insert_idx].key, key, MAX_KEY_LEN - 1);
      map->entries[insert_idx].key[MAX_KEY_LEN - 1] = '\0';
      map->entries[insert_idx].val = val;
      map->len++;
      if (first_deleted < 0)
        map->growth_left--;
      return;
    }

    idx = (idx + 1) % map->cap;
    probes++;
  }

  if (first_deleted >= 0) {
    map->ctrl[first_deleted] = tag;
    strncpy(map->entries[first_deleted].key, key, MAX_KEY_LEN - 1);
    map->entries[first_deleted].key[MAX_KEY_LEN - 1] = '\0';
    map->entries[first_deleted].val = val;
    map->len++;
  }
}

int hashmap_remove(HashMap *map, const char *key) {
  if (map->cap == 0)
    return 0;

  uint64_t hash = hash_string(key);
  int8_t tag = (int8_t)(hash & 0x7F);
  int64_t idx = ((hash >> 7) % map->cap);
  int64_t probes = 0;

  while (probes < map->cap) {
    int8_t ctrl = map->ctrl[idx];

    if (ctrl == tag) {
      if (strcmp(map->entries[idx].key, key) == 0) {
        map->ctrl[idx] = CTRL_DELETED;
        map->len--;
        return 1;
      }
    }

    if (ctrl == CTRL_EMPTY)
      return 0;

    idx = (idx + 1) % map->cap;
    probes++;
  }
  return 0;
}

void hashmap_grow(HashMap *map) {
  int64_t old_cap = map->cap;
  int64_t new_cap = (old_cap == 0) ? INITIAL_CAP : old_cap * 2;

  int8_t *new_ctrl = (int8_t *)malloc(new_cap);
  memset(new_ctrl, CTRL_EMPTY, new_cap);
  Entry *new_entries = (Entry *)malloc(new_cap * sizeof(Entry));

  if (old_cap > 0) {
    for (int64_t i = 0; i < old_cap; i++) {
      if (map->ctrl[i] >= 0) {
        Entry e = map->entries[i];
        uint64_t hash = hash_string(e.key);
        int8_t tag = (int8_t)(hash & 0x7F);
        int64_t new_idx = ((hash >> 7) % new_cap);

        while (new_ctrl[new_idx] != CTRL_EMPTY) {
          new_idx = (new_idx + 1) % new_cap;
        }

        new_ctrl[new_idx] = tag;
        new_entries[new_idx] = e;
      }
    }
    free(map->ctrl);
    free(map->entries);
  }

  map->ctrl = new_ctrl;
  map->entries = new_entries;
  map->cap = new_cap;
  map->growth_left = ((new_cap * LOAD_FACTOR_NUM) / LOAD_FACTOR_DEN) - map->len;
}

void hashmap_free(HashMap *map) {
  if (map->cap > 0) {
    free(map->ctrl);
    free(map->entries);
  }
  free(map);
}

int main() {
  int64_t checksum = 0;
  char key_buf[MAX_KEY_LEN];

  for (int iter = 0; iter < 100; iter++) {
    HashMap *map = hashmap_new(16);

    // Insert 1000 string keys
    for (int64_t i = 0; i < 1000; i++) {
      snprintf(key_buf, MAX_KEY_LEN, "key_%lld", (long long)i);
      hashmap_insert(map, key_buf, i * 7);
    }

    // Lookup all keys
    for (int64_t i = 0; i < 1000; i++) {
      snprintf(key_buf, MAX_KEY_LEN, "key_%lld", (long long)i);
      checksum += hashmap_get(map, key_buf);
    }

    // Remove half
    for (int64_t i = 0; i < 500; i++) {
      snprintf(key_buf, MAX_KEY_LEN, "key_%lld", (long long)(i * 2));
      hashmap_remove(map, key_buf);
    }

    // Re-insert
    for (int64_t i = 0; i < 500; i++) {
      snprintf(key_buf, MAX_KEY_LEN, "key_%lld", (long long)(i * 2));
      hashmap_insert(map, key_buf, i * 11);
    }

    // Final lookups
    for (int64_t i = 0; i < 1000; i++) {
      snprintf(key_buf, MAX_KEY_LEN, "key_%lld", (long long)i);
      checksum += hashmap_get(map, key_buf);
    }

    hashmap_free(map);
  }

  printf("Checksum: %lld\n", (long long)checksum);
  return 0;
}
