/*
 * C10M Benchmark: Noisy Neighbor (C Baseline)
 * Tests scheduler fairness - simulates 1000Hz vs 60Hz interleaved work
 *
 * This is a CPU-bound simulation without actual async I/O.
 * It measures: context switch overhead, work stealing fairness, jitter.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define RING_BUFFER_SIZE 1024
#define SENSOR_HZ 1000
#define COMPUTE_HZ 60
#define BENCHMARK_DURATION_MS 5000
#define HASHMAP_SIZE 100000
#define LOOKUPS_PER_FRAME 5000000

typedef struct {
  int64_t timestamp;
  float gyro_x, gyro_y, gyro_z;
} SensorData;

typedef struct {
  SensorData data[RING_BUFFER_SIZE];
  int head, tail, count;
} RingBuffer;

// Simple open-addressing hashmap for benchmark
typedef struct {
  int64_t *keys;
  int64_t *vals;
  int capacity;
} HashMap;

// Globals
RingBuffer g_buffer = {0};
int64_t g_max_jitter_us = 0;
int64_t g_sensor_tick_count = 0;
int64_t g_last_sensor_ns = 0;

// Timing helpers
int64_t get_time_ns(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

float pseudo_random_f32(int64_t seed) {
  int64_t x = seed ^ (seed >> 21);
  x = x ^ (x << 35);
  x = x ^ (x >> 4);
  return (float)(x % 1000) / 1000.0f;
}

// Ring buffer operations
void ring_push(RingBuffer *rb, SensorData item) {
  rb->data[rb->head] = item;
  rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
  if (rb->count < RING_BUFFER_SIZE) {
    rb->count++;
  } else {
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
  }
}

int ring_drain(RingBuffer *rb) {
  int drained = rb->count;
  rb->count = 0;
  rb->head = 0;
  rb->tail = 0;
  return drained;
}

// HashMap operations
HashMap *hashmap_new(int capacity) {
  HashMap *m = malloc(sizeof(HashMap));
  m->keys = calloc(capacity, sizeof(int64_t));
  m->vals = calloc(capacity, sizeof(int64_t));
  m->capacity = capacity;
  // Initialize keys to -1 (empty)
  for (int i = 0; i < capacity; i++)
    m->keys[i] = -1;
  return m;
}

void hashmap_insert(HashMap *m, int64_t key, int64_t val) {
  int idx = (int)(key % m->capacity);
  while (m->keys[idx] != -1 && m->keys[idx] != key) {
    idx = (idx + 1) % m->capacity;
  }
  m->keys[idx] = key;
  m->vals[idx] = val;
}

int64_t hashmap_get(HashMap *m, int64_t key) {
  int idx = (int)(key % m->capacity);
  int start = idx;
  while (m->keys[idx] != -1) {
    if (m->keys[idx] == key)
      return m->vals[idx];
    idx = (idx + 1) % m->capacity;
    if (idx == start)
      break;
  }
  return 0;
}

void hashmap_free(HashMap *m) {
  free(m->keys);
  free(m->vals);
  free(m);
}

// Simulate 1000Hz sensor ingest (should take <1ms)
void sensor_ingest(int64_t now) {
  if (g_last_sensor_ns > 0) {
    int64_t delta_ns = now - g_last_sensor_ns;
    int64_t expected_ns = 1000000; // 1ms
    int64_t jitter_ns = delta_ns > expected_ns ? delta_ns - expected_ns
                                               : expected_ns - delta_ns;
    int64_t jitter_us = jitter_ns / 1000;
    if (jitter_us > g_max_jitter_us) {
      g_max_jitter_us = jitter_us;
    }
  }
  g_last_sensor_ns = now;
  g_sensor_tick_count++;

  SensorData data = {
      .timestamp = now,
      .gyro_x = pseudo_random_f32(now),
      .gyro_y = pseudo_random_f32(now + 1),
      .gyro_z = pseudo_random_f32(now + 2),
  };
  ring_push(&g_buffer, data);
}

// Simulate 60Hz heavy compute frame (can take ~16ms)
int64_t heavy_compute(HashMap *map) {
  // Perform LOOKUPS_PER_FRAME hashmap lookups
  int64_t sum = 0;
  for (int64_t i = 0; i < LOOKUPS_PER_FRAME; i++) {
    int64_t key = i % HASHMAP_SIZE;
    sum += hashmap_get(map, key);
  }

  // Drain sensor buffer
  int drained = ring_drain(&g_buffer);
  return sum + drained;
}

int main(void) {
  printf("=== C10M Noisy Neighbor Benchmark (C) ===\n");
  printf("Testing: 1000Hz sensor vs 60Hz heavy compute\n");
  printf("Duration: %d ms\n\n", BENCHMARK_DURATION_MS);

  // Pre-create hashmap with data
  HashMap *map = hashmap_new(HASHMAP_SIZE * 2);
  for (int64_t i = 0; i < HASHMAP_SIZE; i++) {
    hashmap_insert(map, i, i * 2);
  }

  int64_t start_ns = get_time_ns();
  int64_t end_target_ns = start_ns + (BENCHMARK_DURATION_MS * 1000000LL);

  int64_t next_sensor_ns = start_ns;
  int64_t next_compute_ns = start_ns;
  int64_t sensor_interval_ns = 1000000;   // 1ms for 1000Hz
  int64_t compute_interval_ns = 16666666; // ~16.67ms for 60Hz

  int64_t compute_sum = 0;
  int compute_frames = 0;

  // Main loop - interleave sensor and compute tasks
  while (1) {
    int64_t now = get_time_ns();
    if (now >= end_target_ns)
      break;

    // Check if sensor task is due (higher priority)
    if (now >= next_sensor_ns) {
      sensor_ingest(now);
      next_sensor_ns += sensor_interval_ns;
    }

    // Check if compute task is due
    if (now >= next_compute_ns) {
      compute_sum += heavy_compute(map);
      compute_frames++;
      next_compute_ns += compute_interval_ns;
    }
  }

  int64_t elapsed_ns = get_time_ns() - start_ns;

  // Report results
  printf("=== Results ===\n");
  printf("Sensor ticks: %lld\n", g_sensor_tick_count);
  printf("Expected ticks: ~%d\n", BENCHMARK_DURATION_MS);
  printf("Compute frames: %d (expected: ~%d)\n", compute_frames,
         BENCHMARK_DURATION_MS / 16);
  printf("Max jitter: %lldμs\n", g_max_jitter_us);
  printf("Total time: %.2fms\n", elapsed_ns / 1000000.0);

  // Prevent DCE
  if (compute_sum == 0)
    printf("Unexpected zero sum\n");

  hashmap_free(map);

  // Success criteria: jitter < 1000μs (1ms)
  if (g_max_jitter_us < 1000) {
    printf("✅ PASS: Jitter within 1ms tolerance\n");
    return 0;
  } else {
    printf("❌ FAIL: Jitter exceeded 1ms\n");
    return 1;
  }
}
