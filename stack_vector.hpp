#pragma once
#include "memory.hpp"
#include <cstddef>
#include <iostream>
#include <memory>
#include <type_traits>
#include <utility>
namespace eden {

/* To Do:
 *
 * Ensure CV/Correctness
 * Examine possible UB with reinterpret_cast
 * Implement iterators
template <class T, std::size_t StackBufferSize, class Allocator = allocator<T>>
class StackVector {
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  static constexpr size_t value_size = sizeof(T);
  static constexpr size_t expansion_mod = 2;

  [[no_unique_address]] Allocator m_alloc;
  size_type m_end{0};
  alignas(T) std::byte m_stack_buffer[StackBufferSize * value_size];
  T *m_begin_heap{nullptr};
  T *m_heap_capacity_end{nullptr};

  constexpr void destroy_heap() noexcept(std::is_nothrow_destructible_v<T>) {
    if (m_begin_heap) {
      for (size_type i{}; i < m_end - StackBufferSize; ++i)
        std::destroy_at(&m_begin_heap[i]);
    }

    m_alloc.deallocate(m_begin_heap, m_heap_capacity_end - m_begin_heap);
    m_begin_heap = nullptr;
    m_heap_capacity_end = nullptr;
  }

  constexpr void destroy_stack() noexcept(std::is_nothrow_destructible_v<T>) {
    const size_type num_on_stack = std::min(m_end, StackBufferSize);

    for (size_type i{}; i < num_on_stack; ++i)
      std::destroy_at(
          std::launder(reinterpret_cast<T *>(m_stack_buffer + i * value_size)));
  }

  constexpr void
  construct_stack_move(size_type count, std::byte *other_stack) noexcept(
      std::is_nothrow_move_constructible_v<T>) {
    const size_type num_on_stack = std::min(count, StackBufferSize);
    for (; m_end < num_on_stack; ++m_end)
      std::construct_at(
          reinterpret_cast<T *>(m_stack_buffer + m_end * value_size),
          std::move_if_noexcept(other_stack[m_end * value_size]));
  }

  constexpr void construct_buffers_copy(
      size_type count, std::byte *other_stack,
      T *other_heap) noexcept(std::is_nothrow_copy_constructible_v<T>) {
    const size_type overflow =
        count > StackBufferSize ? count - StackBufferSize : 0;
    const size_type num_on_stack = std::min(count, StackBufferSize);
    for (; m_end < num_on_stack; ++m_end)
      std::construct_at(
          reinterpret_cast<T *>(m_stack_buffer + m_end * value_size),
          other_stack[m_end * value_size]);

    if (overflow) {
      m_begin_heap = m_alloc.allocate(overflow);
      for (size_type i{}; i < overflow; ++i) {
        std::construct_at(m_begin_heap + i, other_heap[i]);
        ++m_end;
      }
      m_heap_capacity_end = m_begin_heap + overflow;
    }
  }

  constexpr void construct_buffers(size_type count, const T &value) noexcept(
      std::is_nothrow_copy_constructible_v<T>) {
    const size_type overflow =
        count > StackBufferSize ? count - StackBufferSize : 0;
    const size_type num_on_stack = std::min(count, StackBufferSize);
    for (; m_end < num_on_stack; ++m_end)
      std::construct_at(
          reinterpret_cast<T *>(m_stack_buffer + m_end * value_size), value);

    if (overflow) {
      m_begin_heap = m_alloc.allocate(overflow);
      for (size_type i{}; i < overflow; ++i) {
        std::construct_at(&m_begin_heap[i], value);
        ++m_end;
      }
      m_heap_capacity_end = m_begin_heap + overflow;
    }
  }

  constexpr void construct_buffers(size_type count) noexcept(
      std::is_nothrow_constructible_v<T>) {
    const size_type overflow =
        count > StackBufferSize ? count - StackBufferSize : 0;
    const size_type num_on_stack = std::min(count, StackBufferSize);
    for (; m_end < num_on_stack; ++m_end)
      std::construct_at(&m_stack_buffer[m_end * value_size]);

    if (overflow) {
      m_begin_heap = m_alloc.allocate(overflow);
      for (size_type i{}; i < overflow; ++i) {
        std::construct_at(&m_begin_heap[i]);
        ++m_end;
      }
      m_heap_capacity_end = m_begin_heap + overflow;
    }
  }

  constexpr void expand() noexcept(std::is_nothrow_move_constructible_v<T> &&
                                   noexcept(Allocator::allocate)) {
    const size_type old_size =
        m_begin_heap ? m_heap_capacity_end - m_begin_heap : 1;

    T *const new_begin = m_alloc.allocate(old_size * expansion_mod);
    T *const new_heap_end = new_begin + old_size * expansion_mod;

    if (old_size != 1)
      for (size_type i{}; i < old_size; ++i)
        std::construct_at(new_begin + i,
                          std::move_if_noexcept(m_begin_heap[i]));

    destroy_heap();
    m_begin_heap = new_begin;
    m_heap_capacity_end = new_heap_end;
  }

public:
  explicit constexpr StackVector() noexcept(noexcept(Allocator()))
      : StackVector(Allocator()) {}

  explicit constexpr StackVector(const Allocator &alloc) : m_alloc(alloc) {}
  explicit StackVector(size_type count, const Allocator &alloc = Allocator())
      : m_alloc(alloc) {
    construct_buffers(count);
  }

  constexpr StackVector(size_type count, const T &value, const Allocator &alloc = Allocator()) {
    construct_buffers(count, value);
  }
  StackVector(std::initializer_list<T> init, const Allocator &alloc = Allocator());

  constexpr StackVector(const StackVector &other) : m_alloc(other.m_alloc) {
    construct_buffers_copy(other.m_end + 1, other.m_stack_buffer,
                           other.m_begin_heap);
  }

  constexpr StackVector(StackVector &&other) noexcept
      : m_alloc(std::move_if_noexcept(other.m_alloc)), m_end(other.m_end),
        m_begin_heap(other.m_begin_heap),
        m_heap_capacity_end(other.m_heap_capacity_end) {
    construct_stack_move(other.m_end + 1, other.m_stack_buffer);

    other.m_begin_heap = nullptr;
    other.m_heap_capacity_end = nullptr;
  }

  constexpr ~StackVector() noexcept(noexcept(std::declval<T>().~T())) {
    destroy_stack();
    destroy_heap();
  }

  constexpr StackVector &operator=(const StackVector &other) {
    destroy_stack();
    destroy_heap();
    m_alloc = other.m_alloc;

    construct_buffers_copy(other.m_end + 1, other.m_stack_buffer,
                           other.m_begin_heap);
    return *this;
  }

  constexpr StackVector &operator=(StackVector &&other) noexcept {
    destroy_stack();
    destroy_heap();
    m_alloc = std::move_if_noexcept(other.m_alloc);

    construct_stack_move(other.m_end + 1, other.m_stack_buffer);
    m_begin_heap = other.m_begin_heap;
    m_heap_capacity_end = other.m_heap_capacity_end;
    m_end = other.m_end;
    other.m_begin_heap = nullptr;
    other.m_heap_capacity_end = nullptr;

    return *this;
  }

  constexpr StackVector &operator=(std::initializer_list<T> ilist) noexcept;

  [[nodiscard]] constexpr T &at(size_type pos) {
    if (pos > m_end)
      throw std::runtime_error("element access beyond bounds in stackvector");

    return *this[pos];
  }

  [[nodiscard]] constexpr const T &at(size_type pos) const {
    if (pos > m_end)
      throw std::runtime_error("element access beyond bounds in stackvector");

    return *this[pos];
  }

  [[nodiscard]] constexpr T &operator[](size_type pos) {
    return pos < StackBufferSize ? *std::launder(reinterpret_cast<T *>(
                                       &m_stack_buffer[pos * value_size]))
                                 : m_begin_heap[pos - StackBufferSize];
  }

  [[nodiscard]] constexpr const T &operator[](size_type pos) const {
    return pos < StackBufferSize ? *std::launder(reinterpret_cast<T *>(
                                       &m_stack_buffer[pos * value_size]))
                                 : m_begin_heap[pos - StackBufferSize];
  }

  [[nodiscard]] constexpr T &front() {
    return *std::launder(reinterpret_cast<T *>(m_stack_buffer));
  }

  [[nodiscard]] constexpr const T &front() const {
    return *std::launder(reinterpret_cast<T *>(m_stack_buffer));
  }

  [[nodiscard]] constexpr T &back() {
    if (m_end <= StackBufferSize)
      return *std::launder(
          reinterpret_cast<T *>(&m_stack_buffer[(m_end - 1) * value_size]));

    return m_begin_heap[(m_end - 1) - StackBufferSize];
  }

  [[nodiscard]] constexpr const T &back() const {
    if (m_end <= StackBufferSize)
      return *std::launder(
          reinterpret_cast<T *>(&m_stack_buffer[(m_end - 1) * value_size]));

    return m_begin_heap[(m_end - 1) - StackBufferSize];
  }

  [[nodiscard]] constexpr T *stack_data() noexcept {
    return std::launder(reinterpret_cast<T *>(m_stack_buffer));
  }

  [[nodiscard]] constexpr const T *stack_data() const noexcept {
    return std::launder(reinterpret_cast<const T *>(m_stack_buffer));
  }

  [[nodiscard]] constexpr T *heap_data() noexcept { return m_begin_heap; }
  [[nodiscard]] constexpr const T *heap_data() const noexcept {
    return m_begin_heap;
  }

  [[nodiscard]] constexpr bool is_empty() const noexcept { return m_end == 0; }
  [[nodiscard]] constexpr size_type size() const noexcept { return m_end; }
  [[nodiscard]] constexpr size_type capacity() const noexcept {
    if (m_end <= StackBufferSize)
      return StackBufferSize;

    return StackBufferSize + (m_heap_capacity_end - m_begin_heap);
  }

  constexpr void clear() noexcept {
    destroy_stack();
    destroy_heap();
  }

  constexpr void push_back(const T &value) {
    if (m_end < StackBufferSize) {
      std::construct_at(std::launder(reinterpret_cast<T *>(
                            &m_stack_buffer[m_end * value_size])),
                        value);
      ++m_end;
      return;
    }

    const size_type heap_end_index = m_end - StackBufferSize;
    if (m_begin_heap + heap_end_index == m_heap_capacity_end ||
        m_heap_capacity_end == nullptr)
      expand();

    std::construct_at(m_begin_heap + heap_end_index, value);
    ++m_end;
  }

  constexpr void push_back(T &&value) {
    if (m_end < StackBufferSize) {
      std::construct_at(std::launder(reinterpret_cast<T *>(
                            &m_stack_buffer[m_end * value_size])),
                        std::move_if_noexcept(value));
      ++m_end;
      return;
    }

    const size_type heap_end_index = m_end - StackBufferSize;
    if (m_begin_heap + heap_end_index == m_heap_capacity_end ||
        m_heap_capacity_end == nullptr)
      expand();

    std::construct_at(m_begin_heap + heap_end_index,
                      std::move_if_noexcept(value));
    ++m_end;
  }

  template <class... Args> constexpr T &emplace_back(Args &&...args) {
    if (m_end < StackBufferSize) {
      std::construct_at(std::launder(reinterpret_cast<T *>(
                            &m_stack_buffer[m_end * value_size])),
                        std::forward<Args>(args)...);
      ++m_end;
      return back();
    }

    const size_type heap_end_index = m_end - StackBufferSize;
    if (m_begin_heap + heap_end_index == m_heap_capacity_end ||
        m_heap_capacity_end == nullptr)
      expand();

    std::construct_at(m_begin_heap + heap_end_index,
                      std::forward<Args>(args)...);
    ++m_end;
    return back();
  }

  constexpr void pop_back() {
    --m_end;
    if (m_end <= StackBufferSize - 1) {
      std::destroy_at(std::launder(
          reinterpret_cast<T *>(&m_stack_buffer[m_end * value_size])));
      return;
    }

    const size_type heap_back_index = m_end - StackBufferSize;
    std::destroy_at(m_begin_heap + heap_back_index);
  }

  constexpr void resize(size_type count);

  constexpr void debug_print() {
    std::cout << "Size: " << size() << std::endl;
    std::cout << "Capacity: " << capacity() << std::endl;
    if (m_end == 0)
      return;

    std::cout << "Stack: ";

    const size_type end_stack = std::min(m_end, StackBufferSize);
    for (size_type i{}; i < end_stack; ++i)
      std::cout << *std::launder(
                       reinterpret_cast<T *>(&m_stack_buffer[i * value_size]))
                << " ";

    std::cout << std::endl;
    if (m_end <= StackBufferSize)
      return;

    const size_type heap_end_index = m_end - StackBufferSize;
    std::cout << "Heap: ";
    for (size_type i{}; i < heap_end_index; ++i)
      std::cout << m_begin_heap[i] << " ";

    std::cout << std::endl;
  }
}
*/

} // namespace eden
