
#include <chrono>
#include <iostream>
#include <list>
#include <vector>

struct Node {
  Node *next;
  Node *prev;
  int data;
};

struct RawList {
  Node *head = nullptr;
  Node *tail = nullptr;

  void push_back(int val) {
    Node *node = new Node{nullptr, tail, val};
    if (!tail) {
      head = tail = node;
    } else {
      tail->next = node;
      tail = node;
    }
  }
};

int main() {
  const int iterations = 1000000;

  // 1. C++ std::list
  std::list<int> cpp_list;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    cpp_list.push_back(i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  std::cout << "C++ std::list: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << " ms" << std::endl;

  // 2. C++ Raw Pointers
  RawList raw_list;
  start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; ++i) {
    raw_list.push_back(i);
  }
  end = std::chrono::high_resolution_clock::now();
  std::cout << "C++ Raw Pointers: "
            << std::chrono::duration_cast<std::chrono::milliseconds>(end -
                                                                     start)
                   .count()
            << " ms" << std::endl;

  return 0;
}
