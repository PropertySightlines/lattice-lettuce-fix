// ml_bridge.c - C Bridge for ML Training Pipeline
//
// Provides:
// - mmap_file(): Zero-copy file loading
// - bench_clock(): Nanosecond timing
// - print_i64/print_f64: Non-variadic printf wrappers
// - write_file(): Binary weight export

#include <errno.h>
#include <fcntl.h>
#include <mach/mach_time.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Nanosecond-precision clock using mach_absolute_time (macOS ARM64)
static mach_timebase_info_data_t timebase_info;
static int timebase_initialized = 0;

int64_t bench_clock(void) {
  if (!timebase_initialized) {
    mach_timebase_info(&timebase_info);
    timebase_initialized = 1;
  }
  uint64_t ticks = mach_absolute_time();
  return (int64_t)(ticks * timebase_info.numer / timebase_info.denom);
}

// Zero-copy file mapping
void *mmap_file(const char *path, uint64_t size) {
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "mmap_file: failed to open %s\n", path);
    return NULL;
  }

  void *ptr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  if (ptr == MAP_FAILED) {
    fprintf(stderr, "mmap_file: mmap failed for %s\n", path);
    return NULL;
  }

  return ptr;
}

void munmap_file(void *ptr, uint64_t size) {
  if (ptr) {
    munmap(ptr, size);
  }
}

// POSIX Syscall Wrappers (sys_open, sys_close, sys_read, sys_write, sys_mmap,
// sys_munmap) are provided by runtime.c — do not duplicate here.

// Write binary data to file
int32_t write_file(const char *path, const void *data, uint64_t size) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "write_file: failed to open %s\n", path);
    return -1;
  }

  size_t written = fwrite(data, 1, size, f);
  fclose(f);

  return (written == size) ? 0 : -1;
}

// Non-variadic printf wrappers (avoids ARM64 varargs ABI issues)
int32_t print_i64(const char *fmt, int64_t val) {
  return fprintf(stderr, fmt, val);
}

int32_t print_f64(const char *fmt, double val) { return printf(fmt, val); }

int32_t print_f32(const char *fmt, float val) {
  return printf(fmt, (double)val);
}

// Allocate large page for region memory
void *alloc_page(void) {
  void *ptr = NULL;
  if (posix_memalign(&ptr, 4096, 1024 * 1024) != 0) {
    fprintf(stderr, "alloc_page: allocation failed\n");
    exit(1);
  }
  return ptr;
}

// --- Salt Runtime Bypasses ---

// Simple allocator wrapper - returns pointer to preserve provenance
void *alloc(uint64_t bytes) {
  void *ptr = NULL;
  // Ensure 64-byte alignment for SIMD safety
  if (posix_memalign(&ptr, 64, bytes) != 0) {
    fprintf(stderr, "alloc: posix_memalign failed\n");
    exit(1);
  }
  return ptr;
}

// Fill with zeros
void salt_zeros_f32(float *ptr, uint64_t count) {
  for (uint64_t i = 0; i < count; i++) {
    ptr[i] = 0.0f;
  }
}

// Random normal initialization (simplified)
void salt_randn_f32(float *ptr, uint64_t count, float scale, uint32_t seed) {
  for (uint64_t i = 0; i < count; i++) {
    float r = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
    ptr[i] = r * scale;
  }
}

// Tensor Addition: c = a + b
void salt_add(float *a, float *b, float *c, uint64_t count) {
  for (uint64_t i = 0; i < count; i++) {
    c[i] = a[i] + b[i];
  }
}

// Matrix Multiplication: res = lhs * rhs
// LHS: [M x K], RHS: [K x N], Res: [M x N]
void salt_matmul(float *lhs, float *rhs, float *res, uint64_t m, uint64_t k,
                 uint64_t n) {
  // Naive triple loop
  for (uint64_t i = 0; i < m; i++) {
    for (uint64_t j = 0; j < n; j++) {
      float sum = 0.0f;
      for (uint64_t l = 0; l < k; l++) {
        // Access tensors assumed row-major contiguous
        float val_a = lhs[i * k + l];
        float val_b = rhs[l * n + j];
        sum += val_a * val_b;
      }
      res[i * n + j] = sum;
    }
  }
}

// Matrix-Vector Multiplication: res = lhs * rhs
// LHS: [M x K], RHS: [K], Res: [M]
void salt_matvec(float *lhs, float *rhs, float *res, uint64_t m, uint64_t k) {
  for (uint64_t i = 0; i < m; i++) {
    float sum = 0.0f;
    for (uint64_t l = 0; l < k; l++) {
      sum += lhs[i * k + l] * rhs[l];
    }
    res[i] = sum;
  }
}

// Outer product weight update: W[rows, cols] -= lr * d[rows] * x[cols]
void salt_outer_update(float *w, float *d, float *x, uint64_t rows,
                       uint64_t cols, float lr) {
  for (uint64_t r = 0; r < rows; r++) {
    float d_val = d[r];
    float scaled_d = lr * d_val;
    float *row = w + r * cols;
    for (uint64_t c = 0; c < cols; c++) {
      row[c] -= scaled_d * x[c];
    }
  }
}

// ========== Confusion Matrix Tracking ==========
// For per-class precision/recall/F1 computation

#define NUM_CLASSES 16

static int confusion_matrix[NUM_CLASSES][NUM_CLASSES];
static int class_true_positives[NUM_CLASSES];
static int class_false_positives[NUM_CLASSES];
static int class_false_negatives[NUM_CLASSES];

void reset_confusion(void) {
  for (int i = 0; i < NUM_CLASSES; i++) {
    class_true_positives[i] = 0;
    class_false_positives[i] = 0;
    class_false_negatives[i] = 0;
    for (int j = 0; j < NUM_CLASSES; j++) {
      confusion_matrix[i][j] = 0;
    }
  }
}

void update_confusion(int32_t label, int32_t pred) {
  if (label >= 0 && label < NUM_CLASSES && pred >= 0 && pred < NUM_CLASSES) {
    confusion_matrix[label][pred]++;
    if (pred == label) {
      class_true_positives[label]++;
    } else {
      class_false_positives[pred]++;
      class_false_negatives[label]++;
    }
  }
}

void calculate_and_print_metrics(void) {
  float total_precision = 0, total_recall = 0, total_f1 = 0;
  int valid_classes = 0;

  printf("\n  Class  Precision  Recall     F1\n");
  printf("  ------------------------------------\n");

  for (int c = 0; c < 10; c++) { // Only show classes 0-9 for MNIST
    int tp = class_true_positives[c];
    int fp = class_false_positives[c];
    int fn = class_false_negatives[c];

    float prec = (tp + fp > 0) ? (float)tp / (tp + fp) : 0;
    float rec = (tp + fn > 0) ? (float)tp / (tp + fn) : 0;
    float f1 = (prec + rec > 0) ? 2 * prec * rec / (prec + rec) : 0;

    printf("    %d    %.4f     %.4f     %.4f\n", c, prec, rec, f1);

    if (tp + fp + fn > 0) {
      total_precision += prec;
      total_recall += rec;
      total_f1 += f1;
      valid_classes++;
    }
  }

  if (valid_classes > 0) {
    printf("  ------------------------------------\n");
    printf("  Macro  %.4f     %.4f     %.4f\n", total_precision / valid_classes,
           total_recall / valid_classes, total_f1 / valid_classes);
  }
}

// Entry point wrapper for Salt
extern int32_t main_salt(void);

int main(void) { return main_salt(); }
