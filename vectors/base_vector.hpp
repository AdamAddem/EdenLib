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

/*
*   Small(default false):
*     - Reduces the vector's size to 16 bytes on 64bit systems by storing a pointer and two u32s.
*     - Will be disabled regardless if the system has a less than 8 byte pointer size.
*   ExpansionMult(default 2):
*     - The multiplier applied to the capacity everytime the vector must expand.
*/
template <bool Small = false, u64_t ExpansionMult = 2>
requires (ExpansionMult > 1)
struct base_vector_settings {
  static constexpr u64_t expansion_mult = ExpansionMult;
  static constexpr bool is_small = sizeof(void*) >= 8 ? Small : false;
};

/* Note that there is zero exception safety, and copy constructor/assignment are unimplemented currently. */
template <class T, class Derived, base_vector_settings settings = base_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (move_constructible_c<T> or copy_constructible_c<T>) and std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value
class base_vector {
protected:
  static constexpr u64_t expansion_mult = settings.expansion_mult;
  static constexpr auto is_small = settings.is_small;

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
#define call_derived static_cast<Derived*>(this)->
#define call_derived_const static_cast<const Derived*>(this)->

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

    m_alloc.deallocate(m_begin,  call_derived capacity());
    call_derived zero_members();
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
    call_derived allocate_from_empty(count);

    while (m_size not_eq m_cap) {
      if constexpr(is_small)
        alloc_traits::construct(m_alloc, m_begin + m_size++, std::forward<Args>(args)...);
      else
        alloc_traits::construct(m_alloc, m_size++, std::forward<Args>(args)...);
    }
  }

  constexpr void expand_to(count_t count)
  noexcept(nothrow_move_construct or nothrow_copy_construct) {
    assert(count >= call_derived size());
    T* new_buff = m_alloc.allocate(count);
    auto i{0uz};

    if constexpr(is_small) {
      if (count < m_cap) {
        std::print("It seems like a small eden::vector overflowed. Old capacity: {} attempted to expand to {}.", m_cap, count);
        eden_unreachable("oopsie!");
      }
    }

    auto const sz = call_derived size();
    while (i < sz) {
      alloc_traits::construct(m_alloc, new_buff + i, std::move_if_noexcept(m_begin[i]));
      ++i;
    }
    call_derived destroy(); call_derived deallocate();
    m_begin = new_buff;

    if constexpr (is_small) { m_size = sz; m_cap = count; }
    else { m_size = m_begin + i; m_cap = m_begin + count; }
  }

  constexpr void destroy_n_backwards(count_t n)
  noexcept(nothrow_destruct) {
    assume_assert(m_begin); assert(call_derived size() >= n);
    while (n-- > 0) {
      if constexpr (is_small) alloc_traits::destroy(m_alloc, m_begin + --m_size);
      else alloc_traits::destroy(m_alloc, --m_size);
    }
  }

  constexpr ~base_vector()
  noexcept(nothrow_destruct) {
    if (m_begin == nullptr) return;
    call_derived destroy(); call_derived deallocate();
  }
public:

  template<base_vector_settings other>
  static constexpr bool compatible_settings = other.is_small == is_small;

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
    operator[](count_t idx) const noexcept
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
    operator+=(count_t n) noexcept
    {m_ptr += n; return *this;}

    constexpr const_iterator&
    operator-=(count_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(const_iterator lhs, const_iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr const_iterator
    operator+(const_iterator lhs, count_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr const_iterator
    operator+(count_t n, const_iterator rhs) noexcept
    {return iterator(rhs.m_ptr - n);}

    [[nodiscard]] friend constexpr const_iterator
    operator-(const_iterator lhs, count_t n) noexcept
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
    operator[](count_t idx) const noexcept
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
    operator+=(count_t n) noexcept
    {m_ptr += n; return *this;}

    constexpr iterator&
    operator-=(count_t n) noexcept
    {m_ptr -= n; return *this;}

    [[nodiscard]] explicit constexpr
    operator const_iterator() const noexcept
    {return const_iterator(m_ptr);}

    [[nodiscard]] friend constexpr std::ptrdiff_t
    operator-(iterator lhs, iterator rhs) noexcept
    {return lhs.m_ptr - rhs.m_ptr;}

    [[nodiscard]] friend constexpr iterator
    operator+(iterator lhs, count_t n) noexcept
    {return iterator(lhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator+(count_t n, iterator rhs) noexcept
    {return iterator(rhs.m_ptr + n);}

    [[nodiscard]] friend constexpr iterator
    operator-(iterator lhs, count_t n) noexcept
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
  { call_derived allocate_from_empty(N); }

  constexpr explicit
  base_vector(const Allocator &alloc)
  noexcept(std::is_nothrow_copy_constructible_v<Allocator>)
  : m_alloc(alloc) {}

  constexpr explicit
  base_vector(count_t count, const Allocator &alloc = Allocator())
  noexcept(noexcept(allocate_and_construct(count)))
  requires default_constructible
  : m_alloc(alloc)
  { call_derived allocate_and_construct(count); }

  constexpr base_vector(count_t count, T const& value, Allocator const& alloc = Allocator())
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  : m_alloc(alloc)
  { call_derived allocate_and_construct(count, std::forward<T>(value)); }

#define derived_other static_cast<Derived const&>(other)
  constexpr base_vector(base_vector const& other)
  noexcept(nothrow_copy_construct)
  requires copy_constructible {
    call_derived reserve(derived_other.size());
    auto curr = derived_other.cbegin();
    auto const end = derived_other.cend();
    while (curr not_eq end) {
      call_derived emplace_back(*curr);
      ++curr;
    }
  }

  constexpr base_vector&
  operator=(base_vector const& other)
  noexcept(nothrow_copy_construct)
  requires copy_constructible {
    call_derived destroy(); call_derived reserve(derived_other.size());
    auto curr = derived_other.cbegin();
    auto const end = derived_other.cend();
    while (curr not_eq end) {
      call_derived emplace_back(*curr);
      ++curr;
    }
    return *this;
  }
#undef derived_other

  template <base_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr base_vector(base_vector<T, Derived, other_settings, other_allocator> &&other) noexcept
  : m_alloc(std::move(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  { static_cast<Derived&>(other).zero_members(); }

  template <base_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr void swap(base_vector<T, Derived, other_settings, other_allocator>& other) noexcept {
    if constexpr (alloc_traits::propagate_on_container_swap)
      std::swap(m_alloc, other.m_alloc);
    else
      assume_assert(m_alloc not_eq other.m_alloc and "Undefined behavior if this triggers.");

    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  template <base_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr Derived&
  operator=(base_vector<T, Derived, other_settings, other_allocator> &&other) noexcept {
    call_derived destroy(); call_derived deallocate();
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    static_cast<Derived&>(other).zero_members();
    return static_cast<Derived&>(*this);
  }

  [[nodiscard]] constexpr T&
  at(count_t idx) {
    if (idx >= call_derived size())
      throw std::out_of_range(std::format("Element access at index {} in eden::base_vector with size of {}.", idx, call_derived size()));
    return m_begin[idx];
  }

  [[nodiscard]] constexpr const T&
  at(count_t idx) const {
    if (idx >= call_derived_const size())
      throw std::out_of_range(std::format("Element access at index {} in eden::base_vector with size of {}.", idx, call_derived_const size()));
    return m_begin[idx];
  }

  eden_always_inline [[nodiscard]] constexpr T&       operator[](count_t idx)       noexcept { assume_assert(m_begin); assert(idx < call_derived size()); return m_begin[idx]; }
  eden_always_inline [[nodiscard]] constexpr T const& operator[](count_t idx) const noexcept { assume_assert(m_begin); assert(idx < call_derived_const size()); return m_begin[idx]; }

#define derived_iter typename Derived::iterator
#define derived_rev_iter typename Derived::reverse_iterator
#define derived_const_iter typename Derived::const_iterator
#define derived_const_rev_iter typename Derived::const_reverse_iterator

  eden_always_inline [[nodiscard]] constexpr auto begin()         noexcept { return derived_iter(m_begin); }
  eden_always_inline [[nodiscard]] constexpr auto begin()   const noexcept { return derived_const_iter(m_begin); }
  eden_always_inline [[nodiscard]] constexpr auto cbegin()  const noexcept { return derived_const_iter(m_begin); }
  eden_always_inline [[nodiscard]] constexpr auto rbegin()        noexcept { return derived_rev_iter(call_derived end()); }
  eden_always_inline [[nodiscard]] constexpr auto rbegin()  const noexcept { return derived_const_rev_iter(call_derived_const cend()); }
  eden_always_inline [[nodiscard]] constexpr auto crbegin() const noexcept { return derived_const_rev_iter(call_derived_const cend()); }
  eden_always_inline [[nodiscard]] constexpr auto end()           noexcept { if constexpr (is_small) return derived_iter(m_begin + m_size); else return derived_iter(m_size); }
  eden_always_inline [[nodiscard]] constexpr auto end()     const noexcept { if constexpr (is_small) return derived_const_iter(m_begin + m_size); else return derived_const_iter(m_size); }
  eden_always_inline [[nodiscard]] constexpr auto cend()    const noexcept { return call_derived_const end(); }
  eden_always_inline [[nodiscard]] constexpr auto rend()          noexcept { return derived_rev_iter( call_derived begin()); }
  eden_always_inline [[nodiscard]] constexpr auto rend()    const noexcept { return derived_const_rev_iter( call_derived_const cbegin()); }
  eden_always_inline [[nodiscard]] constexpr auto crend()   const noexcept { return derived_const_rev_iter(call_derived_const cbegin()); }

#undef derived_iter
#undef derived_rev_iter
#undef derived_const_iter
#undef derived_const_rev_iter

  eden_always_inline [[nodiscard]] constexpr T&       front()          noexcept { assume_assert(m_begin); return *m_begin; }
  eden_always_inline [[nodiscard]] constexpr T const& front()    const noexcept { assume_assert(m_begin); return *m_begin; }
  eden_always_inline [[nodiscard]] constexpr T&       back()           noexcept { assume_assert(m_size); if constexpr (is_small) return m_begin[m_size - 1]; else return m_size[-1]; }
  eden_always_inline [[nodiscard]] constexpr T const& back()     const noexcept { assume_assert(m_size); if constexpr (is_small) return m_begin[m_size - 1]; else return m_size[-1]; }
  eden_always_inline [[nodiscard]] constexpr T*       data()           noexcept { return m_begin; }
  eden_always_inline [[nodiscard]] constexpr T const* data()     const noexcept { return m_begin; }
  eden_always_inline [[nodiscard]] constexpr bool     empty()    const noexcept { if constexpr(is_small) return m_size == 0; else return m_size == m_begin; }
  eden_always_inline [[nodiscard]] constexpr count_t  size()     const noexcept { if constexpr (is_small) return m_size; else return m_size - m_begin; }
  eden_always_inline [[nodiscard]] constexpr count_t  capacity() const noexcept { if constexpr(is_small) return m_cap; else return m_cap - m_begin; }
  eden_always_inline               constexpr void     clear()          noexcept { call_derived destroy(); }

  eden_always_inline [[nodiscard]] constexpr explicit operator std::span<T>() noexcept { return std::span(m_begin, call_derived size()); }
  eden_always_inline [[nodiscard]] constexpr explicit operator std::span<const T>() const noexcept { return std::span(m_begin, call_derived_const size()); }
  eden_always_inline [[nodiscard]] constexpr std::span<T> to_span() noexcept { return call_derived operator std::span<T>(); }
  eden_always_inline [[nodiscard]] constexpr std::span<const T> to_span() const noexcept { return call_derived_const operator std::span<const T>(); }

  constexpr void reserve(count_t new_capacity) noexcept {
    if (m_begin == nullptr)
       call_derived allocate_from_empty(new_capacity);
    else if (call_derived capacity() < new_capacity)
       call_derived expand_to(new_capacity);
  }

  constexpr void resize(count_t count) noexcept
  requires default_constructible {
    if (m_begin == nullptr) return call_derived allocate_and_construct(count);

    auto const current_size = call_derived size();
    if (current_size >= count)
      return call_derived destroy_n_backwards(current_size - count);

    if (call_derived capacity() < count)
      call_derived expand_to(count);

    auto const num_default{count - call_derived size()};
    count_t i{};
    while (i++ < num_default)
      if constexpr (is_small) alloc_traits::construct(m_alloc, m_begin + m_size++);
      else  alloc_traits::construct(m_alloc, m_size++);
  }

  constexpr void resize(count_t count, const T& value) noexcept
  requires copy_constructible {
    if (m_begin == nullptr)
      return call_derived allocate_and_construct(count, value);

    auto const current_size = call_derived size();
    if (current_size >= count)
      return call_derived destroy_n_backwards(current_size - count);

    if (call_derived capacity() < count)
      call_derived expand_to(count);

    const auto num_default{count - call_derived size()};
    count_t i{};
    while (i++ < num_default)
      if constexpr (is_small) alloc_traits::construct(m_alloc, m_begin + m_size++, value);
      else alloc_traits::construct(m_alloc, m_size++, value);
  }

  constexpr void shrink_to_fit() const noexcept {
    if ( call_derived_const size() not_eq call_derived_const capacity())
      call_derived_const expand_to(call_derived_const size());
  }

  template <class... Args> constexpr T&
  emplace_back(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    if (m_begin == nullptr)
      call_derived allocate_from_empty(1);
    else if (m_size == m_cap)
      call_derived expand_to(call_derived capacity() * expansion_mult);

    assume_assert(m_begin);
    if constexpr (is_small) {
      auto const obj_location = m_begin + m_size; ++m_size;
      alloc_traits::construct(m_alloc, obj_location, std::forward<Args>(args)...);
      return *obj_location;
    }
    else {
      alloc_traits::construct(m_alloc, m_size, std::forward<Args>(args)...);
      return *(m_size++);
    }
  }

  eden_always_inline constexpr void push_back(const T& value)
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  { call_derived emplace_back(std::forward<const T>(value)); }

  eden_always_inline constexpr void push_back(T&& value)
  noexcept(nothrow_move_construct)
  requires move_constructible
  { call_derived emplace_back(std::forward<T>(value)); }

  eden_always_inline constexpr void pop_back()
  noexcept(nothrow_destruct) {
    assume_assert(m_begin); assume_assert(m_size not_eq m_begin);
    if constexpr(is_small) alloc_traits::destroy(m_alloc, m_begin + --m_size);
    else alloc_traits::destroy(m_alloc, --m_size);
  }
};

template <class T, class Derived, base_vector_settings lhs_settings, base_vector_settings rhs_settings, allocator_for_c<T> allocator>
requires base_vector<T, Derived, lhs_settings, allocator>::template compatible_settings<rhs_settings>
[[nodiscard]] constexpr bool
operator==(const base_vector<T, Derived, lhs_settings, allocator>& lhs, const base_vector<T, Derived, rhs_settings, allocator>& rhs) noexcept
requires std::equality_comparable<T> {
  const auto& lhs_derived = static_cast<const Derived&>(lhs);
  const auto& rhs_derived = static_cast<const Derived&>(rhs);
  const auto sz = lhs_derived.size();
  if (sz not_eq rhs_derived.size())
    return false;
  for (auto i{0uz}; i<sz; ++i) {
    if (lhs_derived[i] not_eq rhs_derived[i])
      return false;
  }
  return true;
}

#undef call_derived
#undef call_derived_const
}
