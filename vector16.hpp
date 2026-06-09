#pragma once
#include "macros.hpp"
#include "metaprogramming.hpp"
#include "type_flags.hpp"
#include "typedefs.hpp"

#include <format>
#include <memory>
#include <print>
#include <ranges>

namespace eden {
template <u64_t ExpansionMult = 2,
          bool CString = false>
requires (ExpansionMult > 1)
struct vector16_settings {
  static constexpr u64_t expansion_mult = ExpansionMult;
  static constexpr bool is_string = CString;
};

template <class T, vector16_settings settings = vector16_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (settings.is_string ? (sizeof(T) == 1 and type<T>::is_integral) : true) and
         (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value)
class vector16 {
  static constexpr bool is_string = settings.is_string;
  static constexpr u32_t expansion_mult = settings.expansion_mult;
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
  u32_t m_size{};
  u32_t m_cap{};

  constexpr void allocate_from_empty(u32_t count)
  noexcept(nothrow_allocating) {
    assume_assert(m_begin == nullptr);
    assume_assert(m_size == 0);
    assume_assert(m_cap == 0);

    m_begin = m_alloc.allocate(count);
    m_cap = count;
  }

  constexpr void deallocate()
  noexcept(nothrow_deallocating) {
    if (m_begin == nullptr) return;

    m_alloc.deallocate(m_begin,  m_cap);
    m_begin = nullptr; m_size = 0; m_cap = 0;
  }

  constexpr void destroy()
  noexcept(nothrow_destruct) {
    if (m_begin == nullptr) return;

    while (m_size not_eq 0)
      alloc_traits::destroy(m_alloc, (--m_size) + m_begin);
  }

  template <class ...Args>
  constexpr void allocate_and_construct_with(u32_t count, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    allocate_from_empty(count);
    while (m_size not_eq m_cap)
      alloc_traits::construct(m_alloc, m_size++, std::forward<Args>(args)...);
  }

  constexpr void expand_to(u32_t count)
  noexcept(nothrow_copy_construct)
  requires (move_constructible or copy_constructible) {
    assume_assert(count >= m_size);
    if (count < m_cap) {
      std::print("It seems like eden::vector16 overflowed. Old capacity: {} attempted to expand to {}.", m_cap, count);
      eden_unreachable("oopsie!");
    }

    T* new_buff = m_alloc.allocate(count);
    auto i{0uz};
    while (i < m_size) {
      alloc_traits::construct(m_alloc, new_buff + i, std::move_if_noexcept(*(m_begin + i)));
      ++i;
    }

    destroy(); deallocate();
    m_begin = new_buff;
    m_cap = count;
  }

  constexpr void destroy_n_backwards(u32_t n)
  noexcept(nothrow_destruct) {
    assume_assert(m_begin); assert(m_size >= n);
    while (n-- > 0)
      alloc_traits::destroy(m_alloc, --m_size);

  }

public:

  template<vector16_settings other>
  static constexpr bool compatible_settings = is_string == other.is_string;

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
    operator[](u32_t idx) const noexcept
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
    operator+=(u32_t n) noexcept
    {m_ptr += n; return *this;}

    constexpr const_iterator&
    operator-=(u32_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(const_iterator lhs, const_iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr const_iterator
    operator+(const_iterator lhs, u32_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr const_iterator
    operator+(u32_t n, const_iterator rhs) noexcept
    {return iterator(rhs.m_ptr - n);}

    [[nodiscard]] friend constexpr const_iterator
    operator-(const_iterator lhs, u32_t n) noexcept
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
    operator[](u32_t idx) const noexcept
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
    operator+=(u32_t n) noexcept
    {m_ptr += n; return *this;}

    constexpr iterator&
    operator-=(u32_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] explicit constexpr
    operator const_iterator() const noexcept
    {return const_iterator(m_ptr);}

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(iterator lhs, iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr iterator
    operator+(iterator lhs, u32_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator+(u32_t n, iterator rhs) noexcept
    {return iterator(rhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator-(iterator lhs, u32_t n) noexcept
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

  constexpr vector16()
  noexcept(std::is_nothrow_default_constructible_v<Allocator>) = default;

  template <u32_t N>
  constexpr explicit
  vector16(flags::ReserveInitial<N>)
  noexcept(nothrow_allocating)
  { allocate_from_empty(N); }

  template <u32_t N>
  constexpr explicit
  vector16(const char(&c_str)[N])
  noexcept(nothrow_allocating)
  requires is_string {
    allocate_from_empty(N);
    std::copy_n(c_str, N - 1, m_begin);
    m_size = N - 1;
  }

  constexpr explicit
  vector16(const Allocator &alloc) noexcept
  : m_alloc(alloc) {}

  constexpr explicit
  vector16(u32_t count, const Allocator &alloc = Allocator())
  noexcept(nothrow_default_constructible)
  requires default_constructible
  : m_alloc(alloc) { allocate_and_construct_with(count); }

  constexpr vector16(u32_t count, const T& value, const Allocator &alloc = Allocator())
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  : m_alloc(alloc) { allocate_and_construct_with(count, std::forward<T>(value)); }

  constexpr vector16(const vector16& other)
  noexcept(nothrow_copy_construct)
  requires copy_constructible = delete; // TO DO: THIS

  constexpr vector16& operator=(const vector16& other)
  noexcept(nothrow_copy_construct and nothrow_destruct)
  requires copy_constructible = delete; // TO DO: THIS

  template <vector16_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr vector16(vector16<T, other_settings, other_allocator> &&other) noexcept
  : m_alloc(std::move(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  { other.m_begin = nullptr; other.m_size = other.m_cap = 0; }

  template <vector16_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr void swap(vector16<T, other_settings, other_allocator>& other) noexcept {
    if constexpr (alloc_traits::propagate_on_container_swap)
      std::swap(m_alloc, other.m_alloc);
    else
      assume_assert(m_alloc not_eq other.m_alloc and "Undefined behavior if this triggers.");

    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  constexpr ~vector16()
  noexcept(nothrow_destruct and nothrow_deallocating) {
    if (m_begin == nullptr) return;
    destroy(); deallocate();
  }

  template <vector16_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr vector16&
  operator=(vector16<T, other_settings, other_allocator> &&other) noexcept {
    destroy(); deallocate();
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.m_begin = nullptr; other.m_size = other.m_cap = 0;
    return *this;
  }

  [[nodiscard]] constexpr T&
  at(u32_t pos) {
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::vector16 with size of {}.", pos, size()));
    return *(m_begin + pos);
  }

  [[nodiscard]] constexpr const T&
  at(u32_t pos) const {
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::vector16 with size of {}.", pos, size()));
    return *(m_begin + pos);
  }

  [[nodiscard]] constexpr T&
  operator[](u32_t pos) noexcept
  {assume_assert(m_begin); return *(m_begin + pos);}

  [[nodiscard]] constexpr const T&
  operator[](u32_t pos) const noexcept
  {assume_assert(m_begin); return *(m_begin + pos);}

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
  {return iterator(m_begin + m_size);}

  [[nodiscard]] constexpr const_iterator
  end() const noexcept
  {return const_iterator(m_begin + m_size);}

  [[nodiscard]] constexpr const_iterator
  cend() const noexcept
  {return const_iterator(m_begin + m_size);}

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
  front() noexcept {
    assume_assert(m_begin); assume_assert(m_size >= 1);
    return *m_begin;
  }

  [[nodiscard]] constexpr const T&
  front() const noexcept {
    assume_assert(m_begin); assume_assert(m_size >= 1);
    return *m_begin;
  }

  [[nodiscard]] constexpr T&
  back() noexcept {
    assume_assert(m_begin); assume_assert(m_size >= 1);
    return *(m_begin + (m_size - 1));
  }

  [[nodiscard]] constexpr const T&
  back() const noexcept {
    assume_assert(m_begin); assume_assert(m_size >= 1);
    return *(m_begin + (m_size - 1));
  }

  // Will NOT return a null terminated string.
  [[nodiscard]] constexpr T*
  data() const noexcept { return m_begin; }


  // RAII opt out. Will return a null terminated string if string is chosen.
  // Tuple is: ptr, size, capacity, moved allocator
  [[nodiscard]] constexpr std::tuple<T*, u32_t, u32_t, Allocator>
  dangerous_release() noexcept {
    if constexpr (is_string) {
      if (m_size == m_cap)
        expand_to(m_cap + 1);
      push_back('\0');
    }

    auto ret = std::tuple(m_begin, m_size, m_cap, std::move(m_alloc));
    m_begin = nullptr; m_cap = m_size = 0;
    return ret;
  }

  [[nodiscard]] constexpr explicit
  operator std::span<T>() const noexcept
  { return std::span(m_begin, m_size); }

  [[nodiscard]] constexpr std::span<T>
  to_span() const noexcept
  { return (*this).operator std::span<T>(); }

  [[nodiscard]] constexpr
  operator std::string_view() const noexcept
  requires is_string
  { return std::string_view(m_begin, m_size); }

  [[nodiscard]] constexpr std::string_view
  to_stringview() const noexcept
  requires is_string
  { return (*this).operator std::string_view(); }

  [[nodiscard]] constexpr explicit
  operator std::string() const noexcept
  requires is_string
  { return std::string(m_begin, m_size); }

  [[nodiscard]] constexpr std::string
  to_stdstring() const noexcept
  requires is_string
  { return (*this).operator std::string(); }

  template <u32_t N>
  [[nodiscard]] constexpr bool
  operator==(const char(&c_str)[N]) noexcept
  requires is_string {
    if ((N-1) not_eq m_size)
      return false;

    auto i{0uz};
    while (i < m_size) {
      if (m_begin[i] not_eq c_str[i])
        return false;
      ++i;
    }

    assume_assert(c_str[N-1] == '\0' and "Pass a terminated string to this function, doofus");
    return true;
  }

  [[nodiscard]] constexpr bool
  operator==(const std::string& std_str) const noexcept
  requires is_string
  { return to_stringview() == std::string_view(std_str); }

  [[nodiscard]] constexpr bool
  empty() const noexcept
  { return m_size == 0; }

  [[nodiscard]] constexpr u32_t
  size() const noexcept
  { return m_size; }

  constexpr void reserve(u32_t new_capacity) noexcept {
    if (m_begin == nullptr)
      allocate_from_empty(new_capacity);
    else if (m_cap < new_capacity)
      expand_to(new_capacity);
  }

  constexpr void resize(u32_t count) noexcept
  requires default_constructible {
    if (m_begin == nullptr)
      return allocate_and_construct_with(count);

    if (m_size >= count)
      return destroy_n_backwards(m_size - count);

    if (m_cap < count)
      expand_to(count);

    while (m_size < m_cap)
      alloc_traits::construct(m_alloc, m_begin + m_size++);
  }

  constexpr void resize(u32_t count, const T& value) noexcept
  requires copy_constructible {
    if (m_begin == nullptr)
      return allocate_and_construct(count, value);

    if (m_size >= count)
      return destroy_n_backwards(m_size - count);

    if (capacity() < count)
      expand_to(count);

    while (m_size < m_cap)
      alloc_traits::construct(m_alloc, m_begin + m_size++, value);
  }

  [[nodiscard]] constexpr u32_t
  capacity() const noexcept
  { return m_cap; }

  constexpr void shrink_to_fit() const noexcept {
    if (m_size not_eq m_cap)
      expand_to(m_size);
  }

  constexpr void clear() noexcept { destroy(); }

  template <class... Args> constexpr T&
  emplace_back(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    if (m_begin == nullptr)
      allocate_from_empty(1);
    else if (m_size == m_cap)
      expand_to(m_cap * expansion_mult);

    assume_assert(m_begin);
    alloc_traits::construct(m_alloc, m_begin + m_size, std::forward<Args>(args)...);
    return *(m_begin + m_size++);
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
  {assume_assert(m_begin); assume_assert(m_size not_eq m_begin); alloc_traits::destroy(m_alloc, m_begin + --m_size);}
};

using string16 = vector16<char, vector16_settings<2, true>{}>;

template <class T, vector16_settings lhs_settings, vector16_settings rhs_settings, allocator_for_c<T> allocator>
requires vector16<T, lhs_settings, allocator>::template compatible_settings<rhs_settings>
[[nodiscard]] constexpr bool
operator==(const vector16<T, lhs_settings, allocator>& lhs, const vector16<T, rhs_settings, allocator>& rhs) noexcept
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