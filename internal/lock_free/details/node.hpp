
#pragma once

namespace lock_free::details {
template <typename Value> struct node {
  struct sentinel {};

  struct iterator {
    constexpr iterator(node *start) noexcept;

    auto operator*() -> node &;
    auto operator*() const -> const node &;
    auto operator->() -> node *;
    auto operator->() const -> const node *;
    auto operator++() -> iterator &;
    auto operator++(int) const -> iterator;
    auto operator==(sentinel) const -> bool;

    node *current;
  };

  using value_type = Value;

  node *next;
  Value value;

  template <typename... CtorArgs>
  constexpr node(node *next, CtorArgs &&...ctor_args) noexcept;

  auto begin() -> iterator;
  auto end() -> sentinel;

  auto last() -> node &;

  template <typename... CtorArgs>
  auto construct_value_in_place(CtorArgs &&...ctor_args) -> Value *;
};
} // namespace lock_free::details

// ---
// Iterator implementation
// ---

template <typename Value>
constexpr lock_free::details::node<Value>::iterator::iterator(
    node *start) noexcept
    : current{start} {}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator*() -> node & {
  return *current;
}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator*() const
    -> const node & {
  return *current;
}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator->() -> node * {
  return current;
}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator->() const
    -> const node * {
  return current;
}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator++() -> iterator & {
  current = current->next;
  return *this;
}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator++(int) const
    -> iterator {
  iterator copy{current};
  this->operator++();
  return copy;
}

template <typename Value>
auto lock_free::details::node<Value>::iterator::operator==(sentinel) const
    -> bool {
  return current == nullptr;
}

// ---
// Node implementation
// ---

template <typename Value>
template <typename... CtorArgs>
constexpr lock_free::details::node<Value>::node(
    node *next, CtorArgs &&...ctor_args) noexcept
    : next{next}, value{std::forward<CtorArgs>(ctor_args)...} {}

template <typename Value>
auto lock_free::details::node<Value>::begin() -> iterator {
  return this;
}

template <typename Value>
auto lock_free::details::node<Value>::end() -> sentinel {
  return {};
}

template <typename Value>
auto lock_free::details::node<Value>::last() -> node & {
  auto current = begin();
  auto previous = current;
  while (++current != end())
    previous = current;
  return *previous;
}

template <typename Value>
template <typename... CtorArgs>
auto lock_free::details::node<Value>::construct_value_in_place(
    CtorArgs &&...ctor_args) -> Value * {
  return std::construct_at<Value, CtorArgs...>(
      &value, std::forward<CtorArgs>(ctor_args)...);
}