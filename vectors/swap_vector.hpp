#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../metaprogramming/type_class.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"

#include <bit>
#include <memory>
#include <print>
#include <span>
#include <string>

namespace eden {

/*
*  Stability (default 4, recommended to be a power of 2 for faster integer division):
*     -When a searched for element is found, it is swapped with the element size()/stability + 1 indices closer to the back.
*     -A stability of 1 always swaps the element with the backmost element.
*     -A higher stability is recommended for patterns that consistently search for a larger subset of elements over a longer period of time.
*     -A lower stability is recommended for patterns that search for a smaller subset of elements that changes frequently.
*  ExpansionMult (default 2):
*     -The multiplier applied to the capacity everytime the vector must expand.
*/
template<u64_t Stability = 4,
         u64_t ExpansionMult = 2>
requires (Stability >= 1)
struct swap_vector_settings {
  static constexpr u64_t stability = Stability;
  static constexpr u64_t expansion_mult = ExpansionMult;
};

template <class T, auto BinaryPredicate, auto settings = swap_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value and std::swappable<T>)
class swap_vector {
  static constexpr u64_t stability = settings.stability;
  static constexpr u64_t expansion_mult = settings.expansion_mult;
  static constexpr bool trivially_destructible = std::is_trivially_destructible_v<T>;

  static constexpr bool default_constructible = std::is_default_constructible_v<T>;
  static constexpr bool move_constructible = std::is_move_constructible_v<T>;
  static constexpr bool copy_constructible = std::is_copy_constructible_v<T>;
  static constexpr bool nothrow_default_constructible = std::is_nothrow_default_constructible_v<T>;
  static constexpr bool nothrow_move_construct = std::is_nothrow_move_constructible_v<T>;
  static constexpr bool nothrow_copy_construct = std::is_nothrow_copy_constructible_v<T>;

  [[no_unique_address]] Allocator m_alloc;
  using alloc_traits = std::allocator_traits<Allocator>;
  static constexpr bool stateless_allocator = std::allocator_traits<Allocator>::is_always_equal;
  static constexpr bool nothrow_destruct = noexcept(alloc_traits::destroy(m_alloc, static_cast<T*>(nullptr)));
  static constexpr bool nothrow_allocating = noexcept(m_alloc.allocate(1));
  static constexpr bool nothrow_deallocating = noexcept(m_alloc.deallocate(static_cast<T*>(nullptr), 1));

  T* m_begin{nullptr};
  T* m_size{nullptr};
  T* m_cap{nullptr};

  constexpr void allocate_from_empty(sz_t count)
  noexcept(nothrow_allocating) {
    assume_assert(m_begin == nullptr);
    assume_assert(m_size == nullptr);
    assume_assert(m_cap == nullptr);

    m_size = m_begin = m_alloc.allocate(count);
    m_cap = m_begin + count;
  }

  constexpr void deallocate()
  noexcept(nothrow_deallocating) {
    if (m_begin == nullptr)
      return;

    m_alloc.deallocate(m_begin,  capacity());
    m_cap = m_size = m_begin = nullptr;
  }

  constexpr void destroy()
  noexcept(nothrow_destruct) {
    if (m_begin == nullptr) return;

    while (m_size not_eq m_begin)
      alloc_traits::destroy(m_alloc, --m_size);
  }

  template <class ...Args>
  constexpr void allocate_and_construct(sz_t count, Args&&... args)
  noexcept(nothrow_constructible_with_c<T, Args...>)
  requires constructible_with_c<T, Args...> {
    allocate_from_empty(count);
    while (m_size not_eq m_cap)
      alloc_traits::construct(m_alloc, m_size++, std::forward<Args>(args)...);
  }

  constexpr void expand_to(sz_t count)
  noexcept(nothrow_move_construct or nothrow_copy_construct)
  requires (move_constructible or copy_constructible) {
    assert(count >= size());
    T* new_buff = m_alloc.allocate(count);
    auto i{0uz};
    while ((m_begin + i) not_eq m_size) {
      alloc_traits::construct(m_alloc, new_buff + i, std::move_if_noexcept(*(m_begin + i)));
      ++i;
    }

    destroy(); deallocate();
    m_begin = new_buff;
    m_size = m_begin + i;
    m_cap = m_begin + count;
  }

  constexpr void destroy_n_backwards(sz_t n)
  noexcept(nothrow_destruct) {
    assume_assert(m_begin); assert(size() >= n);
    while (n-- > 0)
      alloc_traits::destroy(m_alloc, --m_size);
  }

public:

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

  constexpr swap_vector()
  noexcept(std::is_nothrow_default_constructible_v<Allocator>) = default;

  template <sz_t N>
  constexpr explicit
  swap_vector(flags::ReserveInitial<N>)
  noexcept(nothrow_allocating)
  { allocate_from_empty(N); }

  constexpr explicit
  swap_vector(const Allocator &alloc) noexcept
  : m_alloc(alloc) {}

  constexpr explicit
  swap_vector(sz_t count, const Allocator &alloc = Allocator())
  noexcept(noexcept(allocate_and_construct(count)))
  : m_alloc(alloc) { allocate_and_construct(count); }

  constexpr swap_vector(sz_t count, const T& value, const Allocator &alloc = Allocator())
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  : m_alloc(alloc) { allocate_and_construct(count, std::forward<T>(value)); }

  constexpr swap_vector(const swap_vector&) = delete; // TODO: This
  constexpr swap_vector& operator=(const swap_vector&) = delete; // TODO: This

  template <auto other_pred, auto other_settings, allocator_for_c<T> other_allocator>
  requires same_c<Allocator, other_allocator>
  constexpr swap_vector(swap_vector<T, other_pred, other_settings, other_allocator> &&other) noexcept
  : m_alloc(std::move(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  { other.m_begin = other.m_size = other.m_cap = nullptr; }

  template <auto other_pred, auto other_settings, allocator_for_c<T> other_allocator>
  requires same_c<Allocator, other_allocator>
  constexpr void swap(swap_vector<T, other_pred, other_settings, other_allocator>& other) noexcept {
    if constexpr (alloc_traits::propagate_on_container_swap)
      std::swap(m_alloc, other.m_alloc);
    else
      assume_assert(m_alloc not_eq other.m_alloc and "Undefined behavior if this triggers.");

    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  constexpr ~swap_vector()
  noexcept(nothrow_destruct and nothrow_deallocating) {
    if (m_begin == nullptr)
      return;

    destroy();
    deallocate();
  }

  template <auto other_pred, auto other_settings, allocator_for_c<T> other_allocator>
  requires same_c<Allocator, other_allocator>
  constexpr swap_vector&
  operator=(swap_vector<T, other_pred, other_settings, other_allocator> &&other) noexcept {
    destroy(); deallocate();
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.m_begin = other.m_size = other.m_cap = nullptr;
    return *this;
  }

  [[nodiscard]] constexpr T&
  at(sz_t pos) {
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::swap_vector with size of {}.", pos, size()));
    return m_begin[pos];
  }

  [[nodiscard]] constexpr const T&
  at(sz_t pos) const {
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::swap_vector with size of {}.", pos, size()));
    return m_begin[pos];
  }

  [[nodiscard]] constexpr T&
  operator[](sz_t pos) noexcept
  {assume_assert(m_begin); return m_begin[pos];}

  [[nodiscard]] constexpr const T&
  operator[](sz_t pos) const noexcept
  {assume_assert(m_begin); return m_begin[pos];}

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
  data() noexcept { return m_begin; }

  [[nodiscard]] constexpr const T*
  data() const noexcept { return m_begin; }

  // RAII opt out.
  // Tuple is: ptr, end_size_ptr, end_capacity_ptr, moved allocator
  [[nodiscard]] constexpr std::tuple<T*, T*, T*, Allocator>
  release_unsafe() noexcept {
    auto ret = std::tuple(m_begin, m_size, m_cap, std::move(m_alloc));
    m_cap = m_size = m_begin = nullptr;
    return ret;
  }

  [[nodiscard]] constexpr explicit
  operator std::span<T>() const noexcept
  { return std::span(m_begin, size()); }

  [[nodiscard]] constexpr std::span<T>
  to_span() const noexcept
  { return (*this).operator std::span<T>(); }

  [[nodiscard]] constexpr bool
  empty() const noexcept
  {return m_size == m_begin;}

  [[nodiscard]] constexpr sz_t
  size() const noexcept
  {return m_size - m_begin;}

  constexpr void reserve(sz_t new_capacity) noexcept {
    if (m_begin == nullptr)
      allocate_from_empty(new_capacity);
    else if (capacity() < new_capacity)
      expand_to(new_capacity);
  }

  constexpr void resize(sz_t count) noexcept
  requires default_constructible {
    if (m_begin == nullptr)
      return allocate_and_construct(count);

    const sz_t current_size = size();
    if (current_size >= count)
      return destroy_n_backwards(current_size - count);

    if (capacity() < count)
      expand_to(count);

    const auto num_default{count - size()};
    auto i{0uz};
    while (i++ < num_default)
      alloc_traits::construct(m_alloc, m_size++);
  }

  constexpr void resize(sz_t count, const T& value) noexcept
  requires copy_constructible {
    if (m_begin == nullptr)
      return allocate_and_construct(count, value);

    const sz_t current_size = size();
    if (current_size >= count)
      return destroy_n_backwards(current_size - count);

    if (capacity() < count)
      expand_to(count);

    const auto num_default{count - size()};
    auto i{0uz};
    while (i++ < num_default)
      alloc_traits::construct(m_alloc, m_size++, value);
  }

  [[nodiscard]] constexpr sz_t
  capacity() const noexcept
  { return m_cap - m_begin; }

  constexpr void shrink_to_fit() const noexcept {
    if (size() not_eq capacity())
      expand_to(size());
  }

  constexpr void clear() noexcept { destroy(); }

  template <class... Args> constexpr T&
  emplace_back(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    if (m_begin == nullptr)
      allocate_from_empty(1);
    else if (m_size == m_cap)
      expand_to(capacity() * expansion_mult);

    alloc_traits::construct(m_alloc, m_size, std::forward<Args>(args)...);
    return *(m_size++);
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
  noexcept(nothrow_destruct)
  { assume_assert(m_begin); assume_assert(m_size not_eq m_begin); alloc_traits::destroy(m_alloc, --m_size); }

  // returns nullptr if not found. pointer is unstable and may point to bad data should this vector expand or another search be called.
  template<class KeyType>
  [[nodiscard]] constexpr T*
  search(KeyType&& key)
  noexcept(nothrow_swappable_c<T>)
  requires (convertible_to_c<decltype(BinaryPredicate(std::declval<T>(), std::forward<KeyType>(key))), bool>) {
    static constexpr bool transposition = stability >= 1;

    auto const sz = size();
    auto n = sz;
    auto const back_bubble = transposition ? sz / stability + 1 : 0;
    while (n-- > 0) {
      auto curr_ptr = m_begin + n;
      bool const hit = BinaryPredicate(*curr_ptr, std::forward<KeyType>(key));
      if (hit) {
        auto const swap_location = (transposition ? m_begin + std::min(back_bubble + n, sz-1) : m_size - 1);
        if (curr_ptr not_eq swap_location) std::swap(*curr_ptr, *swap_location);
        return swap_location;
      }
    }
    return nullptr;
  }

};

template <class T, auto LHS_Pred, auto LHS_Settings, auto RHS_Pred, auto RHS_Settings, allocator_for_c<T> allocator>
[[nodiscard]] constexpr bool
operator==(const swap_vector<T, LHS_Pred, LHS_Settings, allocator>& lhs, const swap_vector<T, RHS_Pred, RHS_Settings, allocator>& rhs) noexcept
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
