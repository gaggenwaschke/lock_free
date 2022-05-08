
#pragma once

#include <atomic>

#include "node.hpp"

namespace lock_free::details {
template <typename Value> class linked_list {
public:
  // Types.
  using node_type = node<Value>;

  // Constructors.
  constexpr linked_list() noexcept;

  // Methods.
  void push(node_type &nodes_to_insert);
  auto pop() -> node_type *;
  auto pop_all() -> node_type *;

private:
  // Members.
  std::atomic<node_type *> root;
};
} // namespace lock_free::details

// ---
// Implementation
// ---

template <typename Value>
constexpr lock_free::details::linked_list<Value>::linked_list() noexcept
    : root{nullptr} {
  static_assert(decltype(root)::is_always_lock_free,
                "Root pointer type must be lock free.");
}

template <typename Value>
void lock_free::details::linked_list<Value>::push(node_type &nodes) {
  auto &last = nodes.last();
  last.next = root.load(std::memory_order_consume);
  while (!root.compare_exchange_weak(
      last.next, &nodes, std::memory_order_release, std::memory_order_acquire))
    ;
}

template <typename Value>
auto lock_free::details::linked_list<Value>::pop() -> node_type * {
  auto expected_root = root.load(std::memory_order_consume);
  while (expected_root &&
         !root.compare_exchange_weak(expected_root, expected_root->next,
                                     std::memory_order_release,
                                     std::memory_order_acquire))
    ;
  if (expected_root)
    expected_root->next = nullptr;
  return expected_root;
}

template <typename Value>
auto lock_free::details::linked_list<Value>::pop_all() -> node_type * {
  return root.exchange(nullptr, std::memory_order_acquire);
}