int COUNTER = 0;

int main(int argc, char **argv) {
  COUNTER = argc;
  for (int i = 0; i < 100000000; ++i) {
    COUNTER = COUNTER + 1;
  }

  if (COUNTER == (100000000 + argc)) {
    return 0;
  }
  return 1;
}
