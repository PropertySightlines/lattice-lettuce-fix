#include <cstdint>

struct Pixel {
  int r, g, b;
};

int main(int argc, char **argv) {
  volatile uintptr_t sum = 0;
  for (int i = 0; i < 100000000; ++i) {
    uintptr_t addr = i + argc;
    Pixel *p = reinterpret_cast<Pixel *>(addr);

    // Explicitly accumulate address to prevent DCE
    sum += reinterpret_cast<uintptr_t>(p);
  }

  if (sum != 0 && argc > 100)
    return 1;
  return 0;
}
