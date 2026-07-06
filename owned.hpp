#pragma once
#include "metaprogramming/concepts.hpp"
#include "string_utils.hpp"
#include "typedefs.hpp"

#include <cstring>
#include <span>
#include <string>
#include <utility>

namespace eden {

template <class T>
class owned_ptr {
  using value_type = std::remove_extent_t<T>;
  using ptr = value_type*; using const_ptr = const value_type*;
  using ref = value_type&; using const_ref = const value_type&;

  static constexpr bool is_array = std::is_array_v<T>;
  static constexpr bool bounded_array = std::is_bounded_array_v<T>;
  static constexpr sz_t array_size = std::extent_v<T, std::rank_v<T> - 1>;
  static constexpr bool elements_comparable = is_array and requires (value_type a, value_type b) { a == b; };
  static constexpr bool is_string = std::is_same_v<value_type, char> and is_array;

  ptr internal{nullptr};

public:
  constexpr            owned_ptr()                  noexcept = default;
  constexpr explicit   owned_ptr(std::nullptr_t)    noexcept {}
  constexpr explicit   owned_ptr(ptr&& mine_now)    noexcept : internal(mine_now) {}
                       owned_ptr(owned_ptr const&)           = delete;
            owned_ptr& operator=(owned_ptr const&)           = delete;

  constexpr            owned_ptr(owned_ptr&& other) noexcept : internal(other.internal) { other.internal = nullptr; }
  constexpr owned_ptr& operator=(owned_ptr&& other) noexcept { internal = other.internal; other.internal = nullptr; return *this; }

  eden_always_inline [[nodiscard]] constexpr ptr       get()                            noexcept                    { return internal; }
  eden_always_inline [[nodiscard]] constexpr const_ptr get()                      const noexcept                    { return internal; }
  eden_always_inline [[nodiscard]] constexpr ptr       release()                        noexcept                    { ptr const res = internal; internal = nullptr; return res; }
  eden_always_inline               constexpr void      reset(ptr mine_now)              noexcept                    { internal = mine_now; }

  eden_always_inline [[nodiscard]] constexpr ref       operator*()                      noexcept                    { return *internal; }
  eden_always_inline [[nodiscard]] constexpr const_ref operator*()                const noexcept                    { return *internal; }
  eden_always_inline [[nodiscard]] constexpr ptr       operator->()                     noexcept                    { return internal; }
  eden_always_inline [[nodiscard]] constexpr const_ptr operator->()               const noexcept                    { return internal; }
  eden_always_inline [[nodiscard]] constexpr bool      operator==(std::nullptr_t) const noexcept                    { return internal == nullptr; }
  eden_always_inline [[nodiscard]] constexpr ref       operator[](sz_t idx)             noexcept requires is_array  { return internal[idx]; }
  eden_always_inline [[nodiscard]] constexpr const_ref operator[](sz_t idx)       const noexcept requires is_array  { return internal[idx]; }
  eden_always_inline [[nodiscard]] constexpr operator std::string_view()          const noexcept requires is_string { if constexpr (bounded_array) return std::string_view(internal, array_size); else return std::string_view(internal); }
  eden_always_inline               constexpr explicit  operator bool()            const noexcept                    { return internal not_eq nullptr; }


  eden_always_inline [[nodiscard]] constexpr explicit operator std::span<value_type, array_size>()      noexcept requires bounded_array { return std::span<value_type, array_size>(internal); }
  eden_always_inline [[nodiscard]] constexpr std::span<value_type> to_dynamic_span(sz_t length)   const noexcept requires is_array { return std::span(internal, length); }

  [[nodiscard]] constexpr bool
  operator==(const owned_ptr& other) const noexcept {
    if constexpr (bounded_array and elements_comparable) {
      if (internal == other.internal)
        return true;
      for (auto i{0uz}; i<array_size; ++i) {
        if (internal[i] not_eq other.internal[i])
          return false;
      }
      return true;
    }
    else if constexpr(is_string)
      return std::strcmp(internal, other.internal);
    else
      return internal == other.internal;
  }

  [[nodiscard]] constexpr bool
  operator==(const_ptr other) const noexcept {
    if constexpr (is_string) {
      return streq(internal, other);
    }
    else
      return internal == other;
  }

  [[nodiscard]] constexpr explicit
  operator std::string() const noexcept
  requires is_string {
    if constexpr (bounded_array)
      return std::string(internal, array_size);
    else
      return std::string(internal);
  }

  [[nodiscard]] constexpr sz_t
  length() const noexcept
  requires is_string {
    if constexpr (bounded_array)
      return array_size - 1;
    else
      return std::strlen(internal);
  }
};

template <class T>
using nullable_owned_ptr = owned_ptr<T>;
template <sz_t N>
using owned_bounded_cstr = owned_ptr<char[N]>;
using owned_cstr = owned_ptr<char[]>;

template <class, bool> struct ContainsIf {};
template <class T>     struct ContainsIf<T, false> {};
template <class T>     struct ContainsIf<T, true> {T m;};

template <class T, sz_t Extent = std::dynamic_extent>
requires (Extent > 0)
class owned_span {
  static constexpr bool dynamicly_sized = (Extent == std::dynamic_extent);
  static constexpr bool is_string = same_c<T, char>;
  static constexpr bool elements_comparable = requires (T a, T b) { a == b; };

  T* internal;
  [[no_unique_address]]
  ContainsIf<sz_t, dynamicly_sized> length;
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

  [[nodiscard]] constexpr iterator begin() noexcept {return iterator(internal);}

  [[nodiscard]] constexpr const_iterator begin() const noexcept {return const_iterator(internal);}

  [[nodiscard]] constexpr const_iterator cbegin() const noexcept {return const_iterator(internal);}

  [[nodiscard]] constexpr reverse_iterator rbegin() noexcept {return reverse_iterator(end());}

  [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {return const_reverse_iterator(cend());}

  [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {return const_reverse_iterator(cend());}

  [[nodiscard]] constexpr iterator end() noexcept {return iterator(internal + size());}

  [[nodiscard]] constexpr const_iterator end() const noexcept {return const_iterator(internal + size());}

  [[nodiscard]] constexpr const_iterator cend() const noexcept {return const_iterator(internal + size());}

  [[nodiscard]] constexpr reverse_iterator rend() noexcept {return reverse_iterator(begin());}

  [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {return const_reverse_iterator(cbegin());}

  [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {return const_reverse_iterator(cbegin());}

  constexpr owned_span() noexcept requires dynamicly_sized : internal(nullptr) {}

  constexpr explicit
  owned_span(T* mine_now, sz_t count) noexcept
  requires dynamicly_sized
  : internal(mine_now), length{count} {}

  constexpr explicit
  owned_span(owned_ptr<T[]> mine_now, sz_t count) noexcept
  requires dynamicly_sized
  : internal(mine_now.release()), length{count} {}

  template <sz_t N>
  constexpr explicit
  owned_span(owned_ptr<T[N]> mine_now) noexcept
  requires (dynamicly_sized ? true : N == Extent)
  : internal(mine_now.release()) {
    if constexpr(dynamicly_sized)
      length.m = N;
  }

  constexpr explicit
  owned_span(owned_cstr str) noexcept
  requires (dynamicly_sized and is_string)
  : internal(str.release())
  { length.m = std::char_traits<char>::length(internal); }

  owned_span(owned_span const&) = delete;
  owned_span& operator=(owned_span const&) = delete;

  template <sz_t OtherExtent>
  constexpr owned_span(owned_span<T, OtherExtent>&& other) noexcept
  requires (dynamicly_sized)
  : internal(other.internal) {
    other.internal = nullptr;
    if constexpr (other.dynamicly_sized)
      length.m = other.length.m;
    else
      length.m = OtherExtent;
  }

  constexpr owned_span(owned_span&& other) noexcept
  requires (not dynamicly_sized)
  : internal(other.internal)
  { other.internal = nullptr; }

  template <sz_t OtherExtent>
  constexpr owned_span&
  operator=(owned_span<T, OtherExtent>&& other) noexcept
  requires dynamicly_sized {
    internal = other.internal;
    other.internal = nullptr;
    if constexpr (other.dynamicly_sized) {
      length.m = other.length.m;
      other.length.m = 0;
    }
    else
      length.m = OtherExtent;

    return *this;
  }

  constexpr owned_span&
  operator=(owned_span&& other) noexcept
  requires (not dynamicly_sized) {
    internal = other.internal;
    other.internal = nullptr;
    return *this;
  }

  constexpr owned_span&
  operator=(owned_cstr str) noexcept
  requires (dynamicly_sized and is_string) {
    internal = str.release();
    length.m = std::char_traits<char>::length(internal);
    return *this;
  }

  eden_always_inline [[nodiscard]] constexpr T&       front()                                   noexcept { return *internal; }
  eden_always_inline [[nodiscard]] constexpr T const& front()                             const noexcept { return *internal; }
  eden_always_inline [[nodiscard]] constexpr T&       back()                                    noexcept { return *(internal + (size() - 1)); }
  eden_always_inline [[nodiscard]] constexpr T const& back()                              const noexcept { return *(internal + (size() - 1)); }
  eden_always_inline [[nodiscard]] constexpr T*       get()                                     noexcept { return internal; }
  eden_always_inline [[nodiscard]] constexpr T const* get()                               const noexcept { return internal; }
  eden_always_inline [[nodiscard]] constexpr T*       release()                                 noexcept { T* retval = internal; internal = nullptr; return retval; }
  eden_always_inline [[nodiscard]] constexpr sz_t     size()                              const noexcept { if constexpr(dynamicly_sized) return length.m; else return Extent; }
  eden_always_inline [[nodiscard]] constexpr bool     empty()                             const noexcept { return size() == 0; }
  eden_always_inline               constexpr void     reset(T* mine_now, sz_t new_length)       noexcept requires dynamicly_sized { length.m = new_length; internal = mine_now; }
  eden_always_inline               constexpr void     reset(T* mine_now)                        noexcept requires (not dynamicly_sized) { internal = mine_now; }
  eden_always_inline [[nodiscard]] constexpr bool     operator==(std::nullptr_t)          const noexcept { return internal == nullptr; }
  eden_always_inline [[nodiscard]] constexpr T&       operator[](sz_t idx)                      noexcept { return internal[idx]; }
  eden_always_inline [[nodiscard]] constexpr T const& operator[](sz_t idx)                const noexcept { return internal[idx]; }
  eden_always_inline [[nodiscard]] constexpr explicit operator bool()                     const noexcept { return internal not_eq nullptr; }
  eden_always_inline [[nodiscard]] constexpr operator std::string_view()                  const noexcept requires is_string { return std::string_view(internal, size()); }


  template <sz_t OtherExtent>
  [[nodiscard]] constexpr bool
  operator==(const owned_span<T, OtherExtent>& other) const noexcept {
    if constexpr (not elements_comparable)
      return internal == other.internal;

    if (size() not_eq other.size())
      return false;
    for (auto i{0uz}; i<size(); ++i) {
      if (internal[i] not_eq other.internal[i])
        return false;
    }
    return true;
  }

  template <sz_t N>
  [[nodiscard]] constexpr bool
  operator==(const char(&c_str)[N]) const noexcept
  requires is_string {
    if (size() not_eq N)
      return false;
    return streq(internal, c_str, N);
  }

  eden_always_inline [[nodiscard]] constexpr bool operator==(const char* c_str) const noexcept requires is_string { return streq(internal, c_str); }

};

template <class T, sz_t Extent = std::dynamic_extent>
using nullable_owned_span = owned_span<T, Extent>;

using owned_stringview = owned_span<char>;

}