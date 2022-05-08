#include <iostream>
#include <lock_free/list.hpp>

int main() {
  constexpr static std::size_t chunk_size{1024u};
  lock_free::list<int> container{chunk_size};
  container.emplace(10);

  for (const auto &value : container)
    std::cout << value << "\n";
  // std::cout << container.pop() << "\n";
  //  std::cout << container.pop() << "\n";

  // container.emplace(21);
  // container.emplace(42);

  // std::cout << container.pop_multiple(256) << "\n";

  return 0;
}
