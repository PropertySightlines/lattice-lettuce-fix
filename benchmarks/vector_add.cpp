#include <iostream>

template <typename T> struct Vec {
  T data;
};

template <typename T> Vec<T> add_generic(Vec<T> a, Vec<T> b) { return a; }

int main(int argc, char **argv) {
  Vec<int> v1 = {argc};
  Vec<int> v2 = {argc};

  int sum = 0;
  for (int i = 0; i < 100000000; ++i) {
    Vec<int> res = add_generic(v1, v2);
    sum += res.data;
  }

  if (sum == (100000000 * argc)) {
    return 0;
  }
  return 1;
}
