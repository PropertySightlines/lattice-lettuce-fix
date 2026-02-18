
#include <array>
#include <iostream>
#include <memory>
#include <string>

struct TrieNode {
  std::array<std::unique_ptr<TrieNode>, 26> children;
  bool is_word = false;
};

void insert(TrieNode &root, const std::string &word) {
  TrieNode *curr = &root;
  for (char c : word) {
    int idx = c - 'a';
    if (!curr->children[idx]) {
      curr->children[idx] = std::make_unique<TrieNode>();
    }
    curr = curr->children[idx].get();
  }
  curr->is_word = true;
}

bool search(TrieNode &root, const std::string &word) {
  TrieNode *curr = &root;
  for (char c : word) {
    int idx = c - 'a';
    if (!curr->children[idx]) {
      return false;
    }
    curr = curr->children[idx].get();
  }
  return curr->is_word;
}

int main() {
  auto root = std::make_unique<TrieNode>();

  insert(*root, "salt");
  insert(*root, "lattice");

  bool res1 = search(*root, "salt");
  bool res2 = search(*root, "salti");

  std::cout << "Search 'salt': " << res1 << std::endl;
  std::cout << "Search 'salti': " << res2 << std::endl;

  return 0;
}
