// Global counter benchmark - tests memory load/store in tight loop
// Uses data-dependent accumulation to prevent loop strength reduction

int COUNTER = 0;
int SCRATCH[256]; // Force memory traffic

int main(int argc, char **argv) {
  // Initialize with dynamic values
  for (int i = 0; i < 256; i++) {
    SCRATCH[i] = i + argc;
  }

  COUNTER = argc;

  // Loop with data-dependent memory access - can't be strength-reduced
  for (long i = 0; i < 50000000; ++i) {
    int idx = COUNTER & 0xFF;         // Data-dependent index
    COUNTER = COUNTER + SCRATCH[idx]; // Load from computed index
    SCRATCH[idx] = COUNTER;           // Store back
  }

  // Prevent DCE - check depends on all iterations
  if (COUNTER != 0) {
    return 0;
  }
  return 1;
}
