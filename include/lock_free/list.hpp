
#pragma once

#include <lock_free/details/linked_list.hpp>

namespace lock_free {
template <
    typename Value,
    typename ValueAllocator =
        std::allocator<typename details::linked_list<Value>::node_type>,
    typename ChunkNodeAllocator = std::allocator<typename details::linked_list<
        typename details::linked_list<Value>::node_type *>::node_type>>
class list {
public:
  // Types.
  using value_type = Value;
  using value_allocator_type = ValueAllocator;
  using chunk_node_allocator_type = ChunkNodeAllocator;

  struct sentinel {};

  /**
   * @brief This iterator does NOT comply with the standard.
   *
   * @details The iterator tracks which items have been consumed.
   *          When destroyed, it places the consumed ones into the
   *          free nodes, while the remaining nodes get placed into
   *          the active nodes.
   */
  class iterator {
  public:
    using list_type = list;
    using node_type = typename details::linked_list<Value>::node_type;

    iterator() = delete;
    iterator(const iterator &) = delete;
    iterator(iterator &&) = delete;

    iterator(list_type &list, node_type *begin_node) noexcept;
    ~iterator();

    auto operator*() const -> const typename node_type::value_type &;
    auto operator++() -> iterator &;
    auto operator==(sentinel) -> bool;

  private:
    using inner_iterator_type = typename node_type::iterator;

    list_type &list;

    node_type *first;
    node_type *last;
    inner_iterator_type inner_iterator;
  };

  // Constructors.
  list() = delete;
  list(std::size_t chunk_size);

  ~list();

  // Methods.
  template <typename... CtorArgs> void emplace(CtorArgs &&...args);
  auto pop() -> iterator;
  auto begin() -> iterator;
  auto end() const -> sentinel;

private:
  // Types.
  using value_allocator_traits = std::allocator_traits<value_allocator_type>;
  using chunk_node_allocator_traits =
      std::allocator_traits<chunk_node_allocator_type>;
  using list_type = details::linked_list<value_type>;

  // Methods.
  /**
   * @brief Allocates free nodes, then returns one of the new nodes.
   *
   * @details The remaining nodes are attached to the free nodes linked_list.
   *
   * @return list_type::node_type&
   */
  auto allocate_free_nodes() -> typename list_type::node_type &;

  // Members.
  value_allocator_type value_allocator;
  chunk_node_allocator_type chunk_node_allocator;
  std::size_t chunk_size;

  list_type active_nodes;
  list_type free_nodes;
  details::linked_list<typename list_type::node_type *> allocated_chunks;

  // Friends.
  friend iterator;
};
} // namespace lock_free

// ---
// Iterator implementation
// ---

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::iterator::iterator(
    list_type &list, node_type *begin_node) noexcept
    : list{list}, first{begin_node}, last{nullptr}, inner_iterator{begin_node} {
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
lock_free::list<Value, ValueAllocator,
                ChunkNodeAllocator>::iterator::~iterator() {
  if (last) {
    // Split list into used and unused nodes.
    last->next = nullptr;
    // Return used nodes.
    list.free_nodes.push(*first);
  }

  if (inner_iterator != std::end(*first)) {
    // Return unused nodes
    list.active_nodes.push(*inner_iterator);
  }
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator,
                     ChunkNodeAllocator>::iterator::operator*() const -> const
    typename node_type::value_type & {
  return inner_iterator->value;
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator,
                     ChunkNodeAllocator>::iterator::operator++() -> iterator & {
  last = &(*inner_iterator);
  ++inner_iterator;
  return *this;
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator,
                     ChunkNodeAllocator>::iterator::operator==(sentinel)
    -> bool {
  return inner_iterator == std::end(*first);
}

// ---
// List implementation
// ---

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::list(
    std::size_t chunk_size)
    : value_allocator{}, chunk_node_allocator{}, chunk_size{chunk_size},
      active_nodes{}, free_nodes{}, allocated_chunks{} {}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::~list() {
  // Destroy all nodes still in use.
  auto nodes_to_destroy = active_nodes.pop_all();
  if (nodes_to_destroy)
    for (auto &node : *nodes_to_destroy)
      value_allocator_traits::destroy(value_allocator, &node);

  // The allocator exposes an argument "n" for deallocation.
  // Even though, this parameter must be the same size as the
  // size parameter used in the allocation call. Therefor
  // a separate list of root_chunks is held to allow proper
  // memory cleanup.
  auto node = allocated_chunks.pop_all();
  while (node != nullptr) {
    // Manually iterate, because the chunks are deallocated here,
    // but the nodes of the allocated list have to be destroyed AND
    // deallocated at the same time.
    auto next = node->next;
    // Deallocate value chunks.
    value_allocator_traits::deallocate(value_allocator, node->value,
                                       chunk_size);
    // Delete the nodes of the allocation list.
    // They to not need to be destroyed, since they only hold a pointer.
    chunk_node_allocator_traits::deallocate(chunk_node_allocator, node, 1);
    node = next;
  }
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
template <typename... CtorArgs>
void lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::emplace(
    CtorArgs &&...args) {
  auto node = free_nodes.pop();
  if (!node)
    node = &allocate_free_nodes();

  node->template construct_value_in_place<CtorArgs...>(
      std::forward<CtorArgs>(args)...);

  active_nodes.push(*node);
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::pop()
    -> iterator {
  return {*this, active_nodes.pop()};
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::begin()
    -> iterator {
  return {*this, active_nodes.pop_all()};
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator, ChunkNodeAllocator>::end() const
    -> sentinel {
  return {};
}

template <typename Value, typename ValueAllocator, typename ChunkNodeAllocator>
auto lock_free::list<Value, ValueAllocator,
                     ChunkNodeAllocator>::allocate_free_nodes() ->
    typename list_type::node_type & {
  auto new_nodes =
      value_allocator_traits::allocate(value_allocator, chunk_size);
  // Disconnect first node.
  new_nodes[0].next = nullptr;
  // Link each new node expect the first one to the next newly allocated one.
  for (auto ii{1}; ii != chunk_size - 1; ++ii)
    new_nodes[ii].next = &new_nodes[ii + 1];
  new_nodes[chunk_size - 1].next = nullptr;

  // Insert into chunk nodes to be able to clean up through deallocate in the
  // end.
  auto allocation_tracking_node =
      chunk_node_allocator_traits::allocate(chunk_node_allocator, 1);
  chunk_node_allocator_traits::construct(
      chunk_node_allocator, allocation_tracking_node, nullptr, new_nodes);
  allocated_chunks.push(allocation_tracking_node[0]);

  // Insert all except the first node into free nodes.
  free_nodes.push(new_nodes[1]);

  return new_nodes[0];
}