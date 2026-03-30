#pragma once
#include "assume_assert.hpp"
#include "owned.hpp"
#include "type_flags.hpp"
#include "typedefs.hpp"
#include <cstring>
#include <format>
#include <memory>

template <class T, class Allocator = std::allocator<T>>
class releasing_vector {
  static constexpr sz_t value_size = sizeof(T);
  static constexpr sz_t expansion_mod = 2;

  static constexpr bool is_string = std::is_same_v<T, char>;
  static constexpr bool nothrow_copy_construct = std::is_nothrow_copy_constructible_v<T>;
  static constexpr bool nothrow_destruct = std::is_nothrow_destructible_v<T>;

  [[no_unique_address]] Allocator m_alloc;
  static constexpr bool nothrow_allocating = noexcept(m_alloc.allocate(1));
  static constexpr bool nothrow_deallocating = noexcept(m_alloc.deallocate(static_cast<T*>(0), 0));

  T* m_begin{nullptr};
  T* m_size{nullptr};
  T* m_cap{nullptr};


  constexpr void deallocate()
  noexcept(nothrow_deallocating) {
    if (m_begin == nullptr)
      return;
    m_alloc.deallocate(m_begin,  capacity() * value_size);
    m_cap = m_size = m_begin = nullptr;
  }

  constexpr void destroy()
  noexcept(std::is_nothrow_destructible_v<T>) {
    if (m_begin == nullptr)
      return;
    while (m_size not_eq m_begin)
      std::destroy_at(m_size--);
    std::destroy_at(m_begin);
  }

  template <class ...Args>
  constexpr void allocate_and_construct(sz_t count, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    m_cap =
      (m_size = m_begin = m_alloc.allocate(count * value_size)) + count;
    while (m_size not_eq m_cap)
      std::construct_at(m_size++, std::forward<Args>(args)...);
  }

  constexpr void expand_to(sz_t count)
  noexcept(std::is_nothrow_copy_constructible_v<T>) {
    assert(count >= size());
    T* new_buff = m_alloc.allocate(count * value_size);
    auto i{0uz};
    while ((m_begin + i) not_eq m_size) {
      std::construct_at(new_buff + i, std::move_if_noexcept(*(m_begin + i)));
      ++i;
    }
    destroy(); deallocate();
    m_begin = new_buff;
    m_size = m_begin + i;
    m_cap = m_begin + count;
  }

public:

  struct const_iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using value_type        = std::remove_cv_t<T>;
    using element_type      = T;

    const_iterator() : m_ptr(nullptr) {}
    explicit const_iterator(T* ptr) : m_ptr(ptr) {}

    [[nodiscard]] constexpr const_iterator&
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

    [[nodiscard]] constexpr const_iterator&
    operator++() noexcept
    {++m_ptr; return *this;}

    [[nodiscard]] constexpr const_iterator
    operator++(int) noexcept
    {const_iterator tmp = *this; ++(*this); return tmp;}

    [[nodiscard]] constexpr const_iterator&
    operator--() noexcept
    {--m_ptr; return *this;}

    [[nodiscard]] constexpr const_iterator
    operator--(int) noexcept
    {const_iterator tmp = *this; --(*this); return tmp;}

    [[nodiscard]] constexpr const_iterator&
    operator+=(sz_t n) noexcept
    {m_ptr += n; return *this;}

    [[nodiscard]] constexpr const_iterator&
    operator-=(sz_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] friend constexpr bool
    operator<=>(const_iterator, const_iterator) noexcept = default;

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(const_iterator lhs, const_iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr const_iterator
    operator+(const_iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr const_iterator
    operator+(sz_t n, const_iterator rhs) noexcept
    {return iterator(n + rhs.m_ptr);}

    [[nodiscard]] friend constexpr const_iterator
    operator-(const_iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr - n);}

    [[nodiscard]] friend constexpr bool
    operator==(const const_iterator& a, const const_iterator& b) noexcept = default;

  private:
    T* m_ptr;
  };

  struct iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using value_type        = std::remove_cv_t<T>;
    using element_type      = T;

    iterator() : m_ptr(nullptr) {}
    explicit iterator(T* ptr) : m_ptr(ptr) {}

    [[nodiscard]] constexpr iterator&
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

    [[nodiscard]] constexpr iterator&
    operator++() noexcept
    {++m_ptr; return *this;}

    [[nodiscard]] constexpr iterator
    operator++(int) noexcept
    {iterator tmp = *this; ++(*this); return tmp;}

    [[nodiscard]] constexpr iterator&
    operator--() noexcept
    {--m_ptr; return *this;}

    [[nodiscard]] constexpr iterator
    operator--(int) noexcept
    {iterator tmp = *this; --(*this); return tmp;}

    [[nodiscard]] constexpr iterator&
    operator+=(sz_t n) noexcept
    {m_ptr += n; return *this;}

    [[nodiscard]] constexpr iterator&
    operator-=(sz_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] constexpr
    operator const_iterator() const noexcept
    {return const_iterator(m_ptr);}

    [[nodiscard]] friend constexpr bool
    operator<=>(iterator, iterator) noexcept = default;

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(iterator lhs, iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr iterator
    operator+(iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator+(sz_t n, iterator rhs) noexcept
    {return iterator(n + rhs.m_ptr);}

    [[nodiscard]] friend constexpr iterator
    operator-(iterator lhs, sz_t n) noexcept
    {return iterator(lhs.m_ptr - n);}

    [[nodiscard]] friend constexpr bool
    operator==(const iterator& a, const iterator& b) noexcept = default;

  private:
    T* m_ptr;
  };

  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = std::reverse_iterator<iterator>;

  constexpr releasing_vector() = default;
  explicit constexpr releasing_vector(const Allocator &alloc)
  noexcept(std::is_copy_constructible_v<Allocator>)
  requires std::is_copy_constructible_v<Allocator>
  : m_alloc(alloc) {}

  template <sz_t N>
  explicit constexpr releasing_vector(reserve_initial_def<N>)
  : m_begin(m_alloc.allocate(N * value_size))
  {m_size = m_begin; m_cap = m_begin + N;}

  explicit releasing_vector(sz_t count, const Allocator &alloc = Allocator())
  noexcept(std::is_nothrow_default_constructible_v<T> and std::is_nothrow_copy_constructible_v<Allocator>)
  : m_alloc(alloc) {allocate_and_construct(count);}

  constexpr releasing_vector(sz_t count, const T &value, const Allocator &alloc = Allocator())
  noexcept(nothrow_copy_construct and std::is_nothrow_copy_constructible_v<Allocator>)
  requires std::is_copy_constructible_v<T> and std::is_copy_constructible_v<Allocator>
  : m_alloc(alloc) {allocate_and_construct(count, std::forward<T>(value));}

  constexpr releasing_vector(const releasing_vector &other) = delete;
  constexpr releasing_vector& operator=(const releasing_vector &other) = delete;

  explicit constexpr releasing_vector(const char* const terminated_string) noexcept
  requires is_string {
    const sz_t sz = std::strlen(terminated_string) + 1;
    m_size = m_begin = m_alloc.allocate(sz * value_size);
    m_cap = m_begin + sz;
    sz_t i{1};
    char c = terminated_string[0];
    while (c not_eq '\0') {
      *(m_size++) = c;
      c = terminated_string[i++];
    }
  }

  explicit constexpr releasing_vector(owned_ptr<T[]> allocated_data, sz_t sz) noexcept {
    m_begin = allocated_data.release();
    m_cap = m_size = (m_begin + sz);
  }

  template <sz_t N>
  explicit constexpr releasing_vector(owned_ptr<T[N]> allocated_data) noexcept {
    m_begin = allocated_data.release();
    m_cap = m_size = (m_begin + N);
  }

  explicit constexpr releasing_vector(std::unique_ptr<T[]> allocated_data, sz_t sz) noexcept {
    m_begin = allocated_data.release();
    m_cap = m_size = (m_begin + sz);
  }

  template <sz_t N>
  explicit constexpr releasing_vector(std::unique_ptr<T[N]> allocated_data) noexcept {
    m_begin = allocated_data.release();
    m_cap = m_size = (m_begin + N);
  }

  constexpr releasing_vector(releasing_vector &&other)
  noexcept(std::is_nothrow_copy_constructible_v<Allocator> or std::is_nothrow_move_constructible_v<Allocator>)
  requires std::is_copy_constructible_v<Allocator> or std::is_move_constructible_v<Allocator>
  : m_alloc(std::move_if_noexcept(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  {other.m_begin = other.m_size = other.m_cap = nullptr;}

  constexpr void swap(releasing_vector& other) noexcept {
    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  constexpr ~releasing_vector()
  noexcept(noexcept(destroy()) and noexcept(deallocate()))
  {destroy(); deallocate();}

  constexpr releasing_vector &operator=(releasing_vector &&other)
  noexcept(std::is_nothrow_move_assignable_v<Allocator>)
  requires std::is_move_assignable_v<Allocator> {
    destroy(); deallocate();
    m_alloc = std::move_if_noexcept(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.m_begin = other.m_size = other.m_cap = nullptr;
    return *this;
  }

  [[nodiscard]] constexpr T&
  at(sz_t pos) {
    assume_assert(m_begin);
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} with size of {} in releasing_vector.", pos, capacity()));
    return *(m_begin + pos);
  }

  [[nodiscard]] constexpr const T&
  at(sz_t pos) const {
    assume_assert(m_begin);
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} with size of {} in releasing_vector.", pos, capacity()));
    return *(m_begin + pos);
  }

  [[nodiscard]] constexpr T&
  operator[](sz_t pos) noexcept
  {assume_assert(m_begin); return *(m_begin + pos);}

  [[nodiscard]] constexpr const T&
  operator[](sz_t pos) const noexcept
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
  {return iterator(m_size);}

  [[nodiscard]] constexpr const_iterator
  end() const noexcept
  {return const_iterator(m_size);}

  [[nodiscard]] constexpr const_iterator
  cend() const noexcept
  {return const_iterator(m_size);}

  [[nodiscard]] constexpr reverse_iterator
  rend() noexcept
  {return reverse_iterator(begin() - 1);}

  [[nodiscard]] constexpr const_reverse_iterator
  rend() const noexcept
  {return const_reverse_iterator(cbegin() - 1);}

  [[nodiscard]] constexpr const_reverse_iterator
  crend() const noexcept
  {return const_reverse_iterator(cbegin() - 1);}

  [[nodiscard]] constexpr T&
  front() noexcept
  {assume_assert(m_begin); return *m_begin;}

  [[nodiscard]] constexpr const T&
  front() const noexcept
  {assume_assert(m_begin); return *m_begin;}

  [[nodiscard]] constexpr T &back()
  {assume_assert(m_size); return *(m_size - 1);}

  [[nodiscard]] constexpr const T &back() const
  {assume_assert(m_size); return *(m_size - 1);}

  [[nodiscard]] constexpr owned_ptr<T[]> release() noexcept {
    assume_assert(m_begin);
    if constexpr (is_string) {
      if (m_size == m_cap)
        expand_to(capacity() + 1);
      push_back('\0');
    }
    auto data = m_begin;
    m_cap = m_size = m_begin = nullptr;
    return owned_ptr<T[]>(data);
  }

  [[nodiscard]] constexpr
  operator std::string_view() noexcept
  requires is_string
  {return std::string_view(m_begin, size());}

  [[nodiscard]] constexpr
  explicit operator std::span<T>() noexcept
  {return std::span(m_begin, size());}

  [[nodiscard]] constexpr
  explicit operator std::string() noexcept
  requires is_string
  {return std::string(m_begin, size());}

  [[nodiscard]] constexpr bool
  operator==(const char* terminated_string) noexcept
  requires is_string {
    if (empty())
      return terminated_string[0] == '\0';
    sz_t i{};
    while (m_begin + i not_eq m_size) {
      const char c = terminated_string[i];
      if (c == '\0')
        return *(m_begin + i) == '\0';
      if (*(m_begin + i) not_eq c)
        return false;
      ++i;
    }
    return terminated_string[i] == '\0';
  }

  [[nodiscard]] constexpr bool empty() const noexcept {return m_size == m_begin;}
  [[nodiscard]] constexpr sz_t size() const noexcept {return m_size - m_begin;}
  constexpr void reserve(sz_t new_capacity) const noexcept
  {if (capacity() < new_capacity) expand_to(new_capacity);}

  constexpr void resize(sz_t count) noexcept
  requires std::is_default_constructible_v<T> {
    const sz_t current_size = size();
    if (current_size == count)
      return;
    if (current_size > count)
      return expand_to(count);

    if (capacity() < count)
      expand_to(count);

    auto num_default{count - size()};
    auto i{0uz};
    while (i < num_default)
      std::construct_at(m_size + i++);
  }

  constexpr void resize(sz_t count, const T& value) noexcept
  requires std::is_copy_constructible_v<T> {
    const sz_t current_size = size();
    if (current_size == count)
      return;
    if (current_size > count)
      return expand_to(count);

    if (capacity() < count)
      expand_to(count);

    auto num_default{count - size()};
    auto i{0uz};
    while (i < num_default)
      std::construct_at(m_size + i++, value);
  }

  [[nodiscard]] constexpr sz_t capacity() const noexcept {return m_cap - m_begin;}

  constexpr void shrink_to_fit() const noexcept {
    if (size() not_eq capacity())
      expand_to(size());
  }

  constexpr void clear() noexcept {destroy();}

  template <class... Args> constexpr T&
  emplace_back(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>) {
    if (m_begin == nullptr) {
      m_size = m_begin = m_alloc.allocate(2 * value_size);
      m_cap = m_begin + 2;
    }
    else if (m_size == m_cap)
      expand_to(capacity());
    std::construct_at(m_size, std::forward<Args>(args)...);
    return *(m_size++);
  }

  constexpr void push_back(const T &value)
  noexcept(nothrow_copy_construct)
  {emplace_back(std::forward<T>(value));}

  constexpr void push_back(T &&value)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  requires std::is_move_constructible_v<T>
  {emplace_back(std::forward<T>(value));}

  constexpr void pop_back()
  noexcept(nothrow_destruct)
  {assume_assert(m_begin); assume_assert(m_size not_eq m_begin); std::destroy_at(m_size--);}
};

using releasing_string = releasing_vector<char>;

template <class T, class Allocator>
[[nodiscard]] constexpr bool
operator==(const releasing_vector<T, Allocator>& lhs, const releasing_vector<T, Allocator>& rhs) noexcept
requires requires (T a, T b) {a == b;} {
  const auto sz = lhs.size();
  if (sz not_eq rhs.size())
    return false;
  for (auto i{0uz}; i<sz; ++i) {
    if (lhs[i] not_eq rhs[i])
      return false;
  }
  return true;
}