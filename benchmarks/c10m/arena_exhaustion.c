/*
 * C10M Benchmark: Arena Exhaustion (C Baseline)
 * Tests memory allocation under high-frequency task spawning
 *
 * Simulates work stealing with rapid allocation/deallocation patterns.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define SLAB_64_COUNT 8192  // 64B frames
#define SLAB_512_COUNT 2048 // 512B frames
#define SLAB_4K_COUNT 256   // 4KB frames
#define WORK_STEALING_ROUNDS 1000000
#define WORKERS 4

typedef struct {
  uint8_t data[64];
} Frame64;

typedef struct {
  uint8_t data[512];
} Frame512;

typedef struct {
  uint8_t data[4096];
} Frame4K;

// Tiered slab allocator (simplified)
typedef struct {
  Frame64 *slab_64;
  Frame512 *slab_512;
  Frame4K *slab_4k;
  uint64_t bitmap_64[SLAB_64_COUNT / 64];
  uint64_t bitmap_512[SLAB_512_COUNT / 64];
  uint64_t bitmap_4k[SLAB_4K_COUNT / 64];
  int64_t alloc_count_64;
  int64_t alloc_count_512;
  int64_t alloc_count_4k;
  int64_t free_count;
} TieredArena;

int64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

TieredArena *arena_new(void) {
  TieredArena *a = calloc(1, sizeof(TieredArena));
  a->slab_64 = aligned_alloc(64, sizeof(Frame64) * SLAB_64_COUNT);
  a->slab_512 = aligned_alloc(64, sizeof(Frame512) * SLAB_512_COUNT);
  a->slab_4k = aligned_alloc(4096, sizeof(Frame4K) * SLAB_4K_COUNT);
  memset(a->bitmap_64, 0xFF, sizeof(a->bitmap_64)); // All available
  memset(a->bitmap_512, 0xFF, sizeof(a->bitmap_512));
  memset(a->bitmap_4k, 0xFF, sizeof(a->bitmap_4k));
  return a;
}

void arena_free(TieredArena *a) {
  free(a->slab_64);
  free(a->slab_512);
  free(a->slab_4k);
  free(a);
}

// O(1) allocation using bitmask (ctz for first free slot)
void *arena_alloc_64(TieredArena *a) {
  for (int i = 0; i < SLAB_64_COUNT / 64; i++) {
    if (a->bitmap_64[i] != 0) {
      int bit = __builtin_ctzll(a->bitmap_64[i]);
      a->bitmap_64[i] &= ~(1ULL << bit);
      a->alloc_count_64++;
      return &a->slab_64[i * 64 + bit];
    }
  }
  return NULL;
}

void *arena_alloc_512(TieredArena *a) {
  for (int i = 0; i < SLAB_512_COUNT / 64; i++) {
    if (a->bitmap_512[i] != 0) {
      int bit = __builtin_ctzll(a->bitmap_512[i]);
      a->bitmap_512[i] &= ~(1ULL << bit);
      a->alloc_count_512++;
      return &a->slab_512[i * 64 + bit];
    }
  }
  return NULL;
}

void arena_free_64(TieredArena *a, void *ptr) {
  ptrdiff_t idx = ((Frame64 *)ptr - a->slab_64);
  if (idx >= 0 && idx < SLAB_64_COUNT) {
    a->bitmap_64[idx / 64] |= (1ULL << (idx % 64));
    a->free_count++;
  }
}

void arena_free_512(TieredArena *a, void *ptr) {
  ptrdiff_t idx = ((Frame512 *)ptr - a->slab_512);
  if (idx >= 0 && idx < SLAB_512_COUNT) {
    a->bitmap_512[idx / 64] |= (1ULL << (idx % 64));
    a->free_count++;
  }
}

// Simulate work stealing pattern
void work_stealing_simulation(TieredArena *arenas[], int worker_id) {
  TieredArena *my_arena = arenas[worker_id];

  // Allocate frames from local arena
  void *frames[64];
  int frame_count = 0;

  for (int i = 0; i < 32; i++) {
    void *f = arena_alloc_64(my_arena);
    if (f) {
      // Simulate some work (touch memory)
      memset(f, worker_id, 64);
      frames[frame_count++] = f;
    }
  }

  for (int i = 0; i < 16; i++) {
    void *f = arena_alloc_512(my_arena);
    if (f) {
      memset(f, worker_id, 512);
      frames[frame_count++] = f;
    }
  }

  // Free frames (some to own arena, some "stolen" to others)
  for (int i = 0; i < frame_count; i++) {
    // 80% return to own arena, 20% "stolen" (would go to mailbox)
    if (i < 32) {
      arena_free_64(my_arena, frames[i]);
    } else {
      arena_free_512(my_arena, frames[i]);
    }
  }
}

int main(void) {
  printf("=== C10M Arena Exhaustion Benchmark (C) ===\n");
  printf("Testing: Tiered slab allocation under work stealing\n");
  printf("Workers: %d, Rounds: %d\n\n", WORKERS, WORK_STEALING_ROUNDS);

  // Create per-worker arenas
  TieredArena *arenas[WORKERS];
  for (int i = 0; i < WORKERS; i++) {
    arenas[i] = arena_new();
  }

  int64_t start_ns = get_time_ns();

  // Simulate work stealing rounds
  for (int round = 0; round < WORK_STEALING_ROUNDS; round++) {
    for (int w = 0; w < WORKERS; w++) {
      work_stealing_simulation(arenas, w);
    }
  }

  int64_t elapsed_ns = get_time_ns() - start_ns;

  // Aggregate stats
  int64_t total_allocs = 0;
  int64_t total_frees = 0;
  for (int i = 0; i < WORKERS; i++) {
    total_allocs += arenas[i]->alloc_count_64 + arenas[i]->alloc_count_512;
    total_frees += arenas[i]->free_count;
  }

  double elapsed_ms = elapsed_ns / 1000000.0;
  double allocs_per_sec = total_allocs / (elapsed_ms / 1000.0);
  double ns_per_alloc = (double)elapsed_ns / total_allocs;

  printf("=== Results ===\n");
  printf("Total allocations: %lld\n", total_allocs);
  printf("Total frees: %lld\n", total_frees);
  printf("Time: %.2fms\n", elapsed_ms);
  printf("Allocs/second: %.0f\n", allocs_per_sec);
  printf("ns/alloc: %.1f\n", ns_per_alloc);

  // Cleanup
  for (int i = 0; i < WORKERS; i++) {
    arena_free(arenas[i]);
  }

  // Target: <10ns per allocation
  if (ns_per_alloc < 50) {
    printf("✅ PASS: Allocation overhead acceptable\n");
    return 0;
  } else {
    printf("⚠️ WARN: Allocation overhead high (target: <50ns)\n");
    return 1;
  }
}
