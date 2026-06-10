#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"

#include <memory>
#include <print>
#include <span>
#include <string>

namespace eden {

// Small will reduce the vector's size to 16 bytes on 64bit systems by storing a pointer and two u32s.
// Will be disabled no matter what if the system has a < 8byte pointer size.
template <bool Small = false, u64_t ExpansionMult = 2>
requires (ExpansionMult > 1)
struct base_vector_settings {
  static constexpr u64_t expansion_mult = ExpansionMult;
  static constexpr bool is_small = sizeof(void*) >= 8 ? Small : false;
};


/* Not meant for real use. This is here so any special implementations of vector don't have to write this boilerplate. */
/* If you intend to use this as a template for your own implementation, note that there is zero exception safety and copy constructor/assignment are unimplemented. */
template <class T, base_vector_settings settings = base_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (move_constructible_c<T> or copy_constructible_c<T>) and std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
class base_vector {
protected:
  static constexpr u64_t expansion_mult = settings.expansion_mult;
  static constexpr bool is_small = settings.is_small;

  static constexpr bool trivially_destructible = std::is_trivially_destructible_v<T>;
  static constexpr bool default_constructible = std::is_default_constructible_v<T>;
  static constexpr bool copy_constructible = std::is_copy_constructible_v<T>;
  static constexpr bool move_constructible = std::is_move_constructible_v<T>;
  static constexpr bool nothrow_default_constructible = std::is_nothrow_default_constructible_v<T>;
  static constexpr bool nothrow_move_construct = std::is_nothrow_move_constructible_v<T>;
  static constexpr bool nothrow_copy_construct = std::is_nothrow_copy_constructible_v<T>;

  [[no_unique_address]] Allocator m_alloc;
  using alloc_traits = std::allocator_traits<Allocator>;
  static constexpr bool stateless_allocator = std::allocator_traits<Allocator>::is_always_equal;
  static constexpr bool nothrow_destruct = noexcept(alloc_traits::destroy(m_alloc, static_cast<T*>(nullptr)));
  static constexpr bool nothrow_allocating = noexcept(m_alloc.allocate(1));
  static constexpr bool nothrow_deallocating = noexcept(m_alloc.deallocate(static_cast<T*>(nullptr), 1));

  T* m_begin{};

  std::conditional_t<is_small, u32_t, T*> m_size{};
  std::conditional_t<is_small, u32_t, T*> m_cap{};

  constexpr void zero_members() noexcept {
    if constexpr (is_small) m_begin = nullptr, m_size = 0, m_cap = 0;
    else m_cap = m_size = m_begin = nullptr;
  }

  using count_t = std::conditional_t<is_small, u32_t, sz_t>;
  constexpr void allocate_from_empty(count_t count)
  noexcept(nothrow_allocating) {
    if constexpr (is_small) {
      assume_assert(m_begin == nullptr);
      assume_assert(m_size == 0);
      assume_assert(m_cap == 0);
      m_begin = m_alloc.allocate(count);
      m_cap = count;
    }
    else {
      assume_assert(m_begin == nullptr);
      assume_assert(m_size == nullptr);
      assume_assert(m_cap == nullptr);
      m_size = m_begin = m_alloc.allocate(count);
      m_cap = m_begin + count;
    }
  }

  constexpr void deallocate()
  noexcept(nothrow_deallocating) {
    if (m_begin == nullptr) return;

    m_alloc.deallocate(m_begin,  capacity());
    zero_members();
  }

  constexpr void destroy()
  noexcept(nothrow_destruct) {
    if (m_begin == nullptr) return;

    if constexpr (is_small) {
      while (m_size not_eq 0) alloc_traits::destroy(m_alloc, (--m_size) + m_begin);
    }
    else {
      while (m_size not_eq m_begin) alloc_traits::destroy(m_alloc, --m_size);
    }
  }

  template <class ...Args>
  constexpr void allocate_and_construct(count_t count, Args&&... args)
  noexcept(nothrow_constructible_with_c<T, Args...>)
  requires constructible_with_c<T, Args...> {
    allocate_from_empty(count);

    while (m_size not_eq m_cap) {
      if constexpr(is_small)
        alloc_traits::construct(m_alloc, m_begin + m_size++, std::forward<Args>(args)...);
      else
        alloc_traits::construct(m_alloc, m_size++, std::forward<Args>(args)...);
    }
  }

  constexpr void expand_to(count_t count)
  noexcept(nothrow_move_construct or nothrow_copy_construct) {
    assert(count >= size());
    T* new_buff = m_alloc.allocate(count);
    auto i{0uz};

    if constexpr(is_small) {
      if (count < m_cap) {
        std::print("It seems like a small eden::vector overflowed. Old capacity: {} attempted to expand to {}.", m_cap, count);
        eden_unreachable("oopsie!");
      }
    }

    auto const sz = size();
    while (i < sz) {
      alloc_traits::construct(m_alloc, new_buff + i, std::move_if_noexcept(*(m_begin + i)));
      ++i;
    }
    destroy(); deallocate();
    m_begin = new_buff;

    if constexpr (is_small) m_cap = count;
    else m_size = m_begin + i, m_cap = m_begin + count;
  }

  constexpr void destroy_n_backwards(count_t n)
  noexcept(nothrow_destruct) {
    assume_assert(m_begin); assert(size() >= n);
    while (n-- > 0)
      if constexpr (is_small) alloc_traits::destroy(m_alloc, m_begin + --m_size);
      else alloc_traits::destroy(m_alloc, --m_size);
  }

public:

  template<base_vector_settings other>
  static constexpr bool compatible_settings = other.is_small = is_small;

  struct const_iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using value_type        = std::remove_cv_t<T>;
    using element_type      = value_type;

    const_iterator() : m_ptr(nullptr) {}
    explicit const_iterator(T* ptr) : m_ptr(ptr) {}

    constexpr const_iterator&
    operator=(const_iterator other) noexcept {
      m_ptr = other.m_ptr;
      return *this;
    }

    [[nodiscard]] constexpr T*
    operator->() const noexcept
    {return m_ptr;}

    [[nodiscard]] constexpr T&
    operator*() const noexcept
    {return *m_ptr;}

    [[nodiscard]] constexpr T&
    operator[](sz_t idx) const noexcept
    {return m_ptr[idx];}

    constexpr const_iterator&
    operator++() noexcept
    {++m_ptr; return *this;}

    constexpr const_iterator
    operator++(int) noexcept
    {const_iterator tmp = *this; ++m_ptr; return tmp;}

    constexpr const_iterator&
    operator--() noexcept
    {--m_ptr; return *this;}

    constexpr const_iterator
    operator--(int) noexcept
    {const_iterator tmp = *this; --m_ptr; return tmp;}

    constexpr const_iterator&
    operator+=(sz_t n) noexcept
    {m_ptr += n; return *this;}

    constexpr const_iterator&
    operator-=(sz_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(const_iterator lhs, const_iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr const_iterator
    operator+(const_iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr const_iterator
    operator+(sz_t n, const_iterator rhs) noexcept
    {return iterator(rhs.m_ptr - n);}

    [[nodiscard]] friend constexpr const_iterator
    operator-(const_iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr - n);}

    [[nodiscard]] friend constexpr bool
    operator==(const const_iterator& a, const const_iterator& b) noexcept = default;

    [[nodiscard]] constexpr auto
    operator<=>(const const_iterator& other) const noexcept
    {return m_ptr <=> other.m_ptr;}

  private:
    T* m_ptr;
  };
  struct iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using value_type        = std::remove_cv_t<T>;
    using element_type      = value_type;

    iterator() : m_ptr(nullptr) {}
    explicit iterator(T* ptr) : m_ptr(ptr) {}

    constexpr iterator&
    operator=(iterator other) noexcept {
      m_ptr = other.m_ptr;
      return *this;
    }

    [[nodiscard]] constexpr T*
    operator->() const noexcept
    {return m_ptr;}

    [[nodiscard]] constexpr T&
    operator*() const noexcept
    {return *m_ptr;}

    [[nodiscard]] constexpr T&
    operator[](sz_t idx) const noexcept
    {return m_ptr[idx];}

    constexpr iterator&
    operator++() noexcept
    {++m_ptr; return *this;}

    constexpr iterator
    operator++(int) noexcept
    {iterator tmp = *this; ++m_ptr; return tmp;}

    constexpr iterator&
    operator--() noexcept
    {--m_ptr; return *this;}

    constexpr iterator
    operator--(int) noexcept
    {iterator tmp = *this; --m_ptr; return tmp;}

    constexpr iterator&
    operator+=(sz_t n) noexcept
    {m_ptr += n; return *this;}

    constexpr iterator&
    operator-=(sz_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] explicit constexpr
    operator const_iterator() const noexcept
    {return const_iterator(m_ptr);}

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(iterator lhs, iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr iterator
    operator+(iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator+(sz_t n, iterator rhs) noexcept
    {return iterator(rhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator-(iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr - n);}

    [[nodiscard]] friend constexpr bool
    operator==(const iterator& a, const iterator& b) noexcept = default;

    [[nodiscard]] constexpr auto
    operator<=>(const iterator& other) const noexcept
    {return m_ptr <=> other.m_ptr;}

  private:
    T* m_ptr;
  };

  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  constexpr base_vector()
  noexcept(std::is_nothrow_default_constructible_v<Allocator>) = default;

  template <count_t N>
  constexpr explicit
  base_vector(flags::ReserveInitial<N>)
  noexcept(nothrow_allocating)
  { allocate_from_empty(N); }

  constexpr explicit
  base_vector(const Allocator &alloc)
  noexcept(std::is_nothrow_copy_constructible_v<Allocator>)
  : m_alloc(alloc) {}

  constexpr explicit
  base_vector(count_t count, const Allocator &alloc = Allocator())
  noexcept(noexcept(allocate_and_construct(count)))
  requires default_constructible
  : m_alloc(alloc) { allocate_and_construct(count); }

  constexpr base_vector(count_t count, const T& value, const Allocator &alloc = Allocator())
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  : m_alloc(alloc) { allocate_and_construct(count, std::forward<T>(value)); }

  constexpr base_vector(const base_vector&) = delete; // TODO: This
  constexpr base_vector& operator=(const base_vector&) = delete; // TODO: This

  template <base_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr base_vector(base_vector<T, other_settings, other_allocator> &&other)
  noexcept(nothrow_move_constructible_c<Allocator>)
  : m_alloc(std::move(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  { other.zero_members(); }

  template <base_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr void swap(base_vector<T, other_settings, other_allocator>& other) noexcept {
    if constexpr (alloc_traits::propagate_on_container_swap)
      std::swap(m_alloc, other.m_alloc);
    else
      assume_assert(m_alloc not_eq other.m_alloc and "Undefined behavior if this triggers.");

    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  constexpr ~base_vector()
  noexcept(nothrow_destruct and nothrow_deallocating) {
    if (m_begin == nullptr) return;
    destroy(); deallocate();
  }

  template <base_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr base_vector&
  operator=(base_vector<T, other_settings, other_allocator> &&other)
  noexcept(nothrow_move_constructible_c<Allocator>) {
    destroy(); deallocate();
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.zero_members();
    return *this;
  }

  [[nodiscard]] constexpr T&
  at(count_t idx) {
    if (m_begin + idx >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::base_vector with size of {}.", idx, size()));
    return m_begin[idx];
  }

  [[nodiscard]] constexpr const T&
  at(count_t idx) const {
    if (m_begin + idx >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::base_vector with size of {}.", idx, size()));
    return m_begin[idx];
  }

  [[nodiscard]] constexpr T&
  operator[](count_t idx) noexcept {
    assume_assert(m_begin); assert(idx < size());
    return m_begin[idx];
  }

  [[nodiscard]] constexpr const T&
  operator[](count_t idx) const noexcept {
    assume_assert(m_begin); assert(idx < size());
    return m_begin[idx];
  }

  [[nodiscard]] constexpr iterator
  begin() noexcept
  {return iterator(m_begin);}

  [[nodiscard]] constexpr const_iterator
  begin() const noexcept
  {return const_iterator(m_begin);}

  [[nodiscard]] constexpr const_iterator
  cbegin() const noexcept
  {return const_iterator(m_begin);}

  [[nodiscard]] constexpr reverse_iterator
  rbegin() noexcept
  {return reverse_iterator(end());}

  [[nodiscard]] constexpr const_reverse_iterator
  rbegin() const noexcept
  {return const_reverse_iterator(cend());}

  [[nodiscard]] constexpr const_reverse_iterator
  crbegin() const noexcept
  {return const_reverse_iterator(cend());}

  [[nodiscard]] constexpr iterator
  end() noexcept
  {return iterator(m_size);}

  [[nodiscard]] constexpr const_iterator
  end() const noexcept
  {return const_iterator(m_size);}

  [[nodiscard]] constexpr const_iterator
  cend() const noexcept
  {return const_iterator(m_size);}

  [[nodiscard]] constexpr reverse_iterator
  rend() noexcept
  {return reverse_iterator(begin());}

  [[nodiscard]] constexpr const_reverse_iterator
  rend() const noexcept
  {return const_reverse_iterator(cbegin());}

  [[nodiscard]] constexpr const_reverse_iterator
  crend() const noexcept
  {return const_reverse_iterator(cbegin());}

  [[nodiscard]] constexpr T&
  front() noexcept
  {assume_assert(m_begin); return *m_begin;}

  [[nodiscard]] constexpr const T&
  front() const noexcept
  {assume_assert(m_begin); return *m_begin;}

  [[nodiscard]] constexpr T&
  back() noexcept
  {assume_assert(m_size); return m_size[-1];}

  [[nodiscard]] constexpr const T&
  back() const noexcept
  {assume_assert(m_size); return m_size[-1];}

  [[nodiscard]] constexpr T*
  data() noexcept
  { return m_begin; }

  [[nodiscard]] constexpr const T*
  data() const noexcept
  { return m_begin; }

  [[nodiscard]] constexpr explicit
  operator std::span<T>() const noexcept
  { return std::span(m_begin, size()); }

  [[nodiscard]] constexpr std::span<T>
  to_span() const noexcept
  { return this->operator std::span<T>(); }

  [[nodiscard]] constexpr bool
  empty() const noexcept {
    if constexpr(is_small) return m_size == 0;
    else return m_size == m_begin;
  }

  [[nodiscard]] constexpr count_t
  size() const noexcept {
    if constexpr (is_small) return m_size;
    else return m_size - m_begin;
  }

  constexpr void reserve(count_t new_capacity) noexcept {
    if (m_begin == nullptr)
      allocate_from_empty(new_capacity);
    else if (capacity() < new_capacity)
      expand_to(new_capacity);
  }

  constexpr void resize(count_t count) noexcept
  requires default_constructible {
    if (m_begin == nullptr) return allocate_and_construct(count);

    auto const current_size = size();
    if (current_size >= count)
      return destroy_n_backwards(current_size - count);

    if (capacity() < count)
      expand_to(count);

    auto const num_default{count - size()};
    count_t i{};
    while (i++ < num_default)
      if constexpr (is_small) alloc_traits::construct(m_alloc, m_begin + m_size++);
      else  alloc_traits::construct(m_alloc, m_size++);
  }

  constexpr void resize(count_t count, const T& value) noexcept
  requires copy_constructible {
    if (m_begin == nullptr)
      return allocate_and_construct(count, value);

    auto const current_size = size();
    if (current_size >= count)
      return destroy_n_backwards(current_size - count);

    if (capacity() < count)
      expand_to(count);

    const auto num_default{count - size()};
    count_t i{};
    while (i++ < num_default)
      if constexpr (is_small) alloc_traits::construct(m_alloc, m_begin + m_size++, value);
      else alloc_traits::construct(m_alloc, m_size++, value);
  }

  [[nodiscard]] constexpr count_t
  capacity() const noexcept {
    if constexpr(is_small) return m_cap;
    else  return m_cap - m_begin;
  }

  constexpr void shrink_to_fit() const noexcept {
    if (size() not_eq capacity())
      expand_to(size());
  }

  constexpr void clear() noexcept
  { destroy(); }

  template <class... Args> constexpr T&
  emplace_back(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    if (m_begin == nullptr)
      allocate_from_empty(1);
    else if (m_size == m_cap)
      expand_to(capacity() * expansion_mult);

    assume_assert(m_begin);
    if constexpr (is_small) {
      alloc_traits::construct(m_alloc, m_begin + m_size, std::forward<Args>(args)...);
      return m_begin[m_size++];
    }
    else {
      alloc_traits::construct(m_alloc, m_size, std::forward<Args>(args)...);
      return *(m_size++);
    }
  }

  constexpr void push_back(const T& value)
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  { emplace_back(std::forward<const T>(value)); }

  constexpr void push_back(T&& value)
  noexcept(nothrow_move_construct)
  requires move_constructible
  { emplace_back(std::forward<T>(value)); }

  constexpr void pop_back()
  noexcept(nothrow_destruct) {
    assume_assert(m_begin); assume_assert(m_size not_eq m_begin);
    if constexpr(is_small) alloc_traits::destroy(m_alloc, m_begin + --m_size);
    else alloc_traits::destroy(m_alloc, --m_size);
  }
};

template <class T, base_vector_settings lhs_settings, base_vector_settings rhs_settings, allocator_for_c<T> allocator>
requires base_vector<T, lhs_settings, allocator>::template compatible_settings<rhs_settings>
[[nodiscard]] constexpr bool
operator==(const base_vector<T, lhs_settings, allocator>& lhs, const base_vector<T, rhs_settings, allocator>& rhs) noexcept
requires std::equality_comparable<T> {
  const auto sz = lhs.size();
  if (sz not_eq rhs.size())
    return false;
  for (auto i{0uz}; i<sz; ++i) {
    if (lhs[i] not_eq rhs[i])
      return false;
  }
  return true;
}

}
