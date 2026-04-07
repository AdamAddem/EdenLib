#pragma once
#include "typedefs.hpp"
#include <cstring>
#include <span>

namespace eden {
/* Owned Pointer:
 *  -Intended to be a wrapper for a raw pointer, with similar semantics to unique_ptr
 *  -Differs from unique_ptr in that it does NOT delete the pointed element on destruction.
 *     -Note: reset also does NOT delete the element.
 *
 *  -This is possibly useful in some edge cases:
 *     -unique_ptr requires the type be complete, so simple methods that could be inlined and done at compile time.
 *      have to be defined in a cpp file rather than the header. owned_ptr does not prevent this.
 *
 *  -Most methods are constexpr and noexcept.
 *  -Array types have some differences and additions listed in the API section.
 *  -c_str is offered as a char[] specialization and offers some extra changes for c-strings.
 *
 * Methods:
 *  -owned_ptr(); //constructs w/ nullptr
 *  -explicit owned_ptr(T*); //takes ownership over pointer
 *
 *  -copy assignment and construction deleted
 *  -move assignment and construction defined
 *
 *  -T* get() const; //returns internal pointer
 *  -T* release(); //releases ownership of internal pointer
 *  -void reset(T*); //releases ownership of previous pointer, takes ownership of parameter
 *    -Important: does not delete previous pointer. Call release or get prior if you need to ensure it is destroyed.
 *
 *  -operator*, ->; //does what you think
 *  -operator==(nullptr_t); //returns true if the internal pointer is null
 *  -operator==(const owned_ptr&), operator==(T*); //returns pointer equality
 *  -operator bool; //returns true if the internal pointer is not null
 *
 * T[] Additions / Modifications:
 *  -If the array type has a bound:
 *    -operator std::span() const; //returns static span of array size
 *    -operator==(const owned_ptr&) const;  //if T is equality comparable, returns true if all elements are equal. otherwise, just compares pointers.
 *  -std::span<T> to_dynamic_span(sz_t length) const; //returns dynamic span with specified length
 *
 * c_str Additions / Modifications:
 *  -operator==(const char*) const; //returns whether std::strcmp determines equal
 *  -operator std::string_view() const; //if bounded_c_str, O(1). Otherwise, O(N)
 *  -operator std::string() const; //if bounded_c_str, O(1). Otherwise, O(N)
 *  -length() const; //returns array bound - 1 if bounded, otherwise strlen()
 */

template <class T>
class owned_ptr {
  using value_type = std::remove_extent_t<T>;
  using ptr = value_type*; using const_ptr = const value_type*;
  using ref = value_type&; using const_ref = const value_type&;

  static constexpr bool is_array = std::is_array_v<T>;
  static constexpr bool bounded_array = std::is_bounded_array_v<T>;
  static constexpr sz_t array_size = std::extent_v<T, std::rank_v<T> - 1>;
  static constexpr bool elements_comparable = is_array and requires (value_type a, value_type b) {a == b;};
  static constexpr bool is_string = std::is_same_v<value_type, char> and is_array;

  ptr internal{nullptr};

public:
  constexpr owned_ptr() noexcept = default;
  constexpr explicit owned_ptr(decltype(nullptr)) noexcept {}

  constexpr explicit owned_ptr(ptr mine_now) noexcept
  : internal(mine_now) {}

  owned_ptr(const owned_ptr&) = delete;
  owned_ptr& operator=(const owned_ptr&) = delete;

  constexpr owned_ptr(owned_ptr&& other) noexcept
  : internal(other.internal) {other.internal = nullptr;}

  constexpr owned_ptr& operator=(owned_ptr&& other) noexcept
  {internal = other.internal; other.internal = nullptr; return *this;}

  [[nodiscard]] constexpr ptr
  get() noexcept
  {return internal;}

  [[nodiscard]] constexpr const_ptr
  get() const noexcept
  {return internal;}

  [[nodiscard]] constexpr ptr
  release() noexcept
  {ptr retval = internal; internal = nullptr; return retval;}

  constexpr void
  reset(ptr mine_now) noexcept
  {internal = mine_now;}

  [[nodiscard]] constexpr ref
  operator*() noexcept
  {return *internal;}

  [[nodiscard]] constexpr const_ref
  operator*() const noexcept
  {return *internal;}

  [[nodiscard]] constexpr ptr
  operator->() noexcept
  {return internal;}

  [[nodiscard]] constexpr const_ptr
  operator->() const noexcept
  {return internal;}

  [[nodiscard]] constexpr bool
  operator==(const decltype(nullptr)) const noexcept
  {return internal == nullptr;}

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
    if constexpr (is_string)
      return std::strcmp(internal, other) == 0;
    else
      return internal == other;
  }

  [[nodiscard]] constexpr ref
  operator[](sz_t idx) noexcept
  requires is_array
  {return internal[idx];}

  [[nodiscard]] constexpr const_ref
  operator[](sz_t idx) const noexcept
  requires is_array
  {return internal[idx];}

  constexpr explicit
  operator bool() const noexcept
  {return internal not_eq nullptr;}

  [[nodiscard]] constexpr explicit
  operator std::span<value_type, array_size>() noexcept
  requires bounded_array
  {return std::span<value_type, array_size>(internal);}

  [[nodiscard]] constexpr std::span<value_type>
  to_dynamic_span(sz_t length) const noexcept
  requires is_array
  {return std::span(internal, length);}

  [[nodiscard]] constexpr explicit
  operator std::string_view() const noexcept
  requires is_string {
    if constexpr (bounded_array)
      return std::string_view(internal, array_size);
    else
      return std::string_view(internal);
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

using c_str = owned_ptr<char[]>;

template <sz_t N>
using bounded_c_str = owned_ptr<char[N]>;

}