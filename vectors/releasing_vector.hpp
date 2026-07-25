#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../metaprogramming/type_class.hpp"
#include "../owned.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"
#include "base_vector.hpp"

#include <memory>
#include <ranges>

namespace eden {
/*
*  StoreSizeAndCapacity (default true, requires T is trivially destructible to be false):
*     - Determines whether the header contains the buffer's size and capacity. Uses 16 extra bytes.
*     - Will allow the released_ptr to be converted into a released_vector.
*     - When false and the allocator is stateless, no header will be created at all, offering the same performance as std::vector.
*  CString (default false):
*     - Determines whether the vector gains extra string functionality, and will ensure the released string is null-terminated.
*  ExpansionMult: (refer to base_vector.hpp)
*/
template <bool StoreSizeAndCapacity = true,
          bool CString = false,
          u64_t ExpansionMult = 2>
requires (ExpansionMult > 1)
struct releasing_vector_settings {
  static constexpr bool is_string = CString;
  static constexpr bool store_size_and_capacity = StoreSizeAndCapacity;
  static constexpr base_vector_settings<false, ExpansionMult> base_settings{};
};

template <class T, auto settings = releasing_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (settings.is_string ? (sizeof(T) == 1 and type<T>::is_integral) : true)
class releasing_vector : public base_vector<T, releasing_vector<T, settings, Allocator>, settings.base_settings, Allocator>  {

  using base = base_vector<T, releasing_vector, settings.base_settings, Allocator>;
  friend class base_vector<T, releasing_vector, settings.base_settings, Allocator>;
  using base::m_begin; using base::m_size; using base::m_cap; using base::m_alloc;
  using base::copy_constructible;
  using base::nothrow_allocating;
  using base::nothrow_deallocating;
  using base::nothrow_move_construct;
  using base::nothrow_copy_construct;
  using base::nothrow_destruct;
  using base::trivially_destructible;

  static constexpr bool is_string = settings.is_string;
  static constexpr bool store_size_and_capacity = settings.store_size_and_capacity;
  static_assert(store_size_and_capacity ? true : trivially_destructible, "Not storing size and capacity is only possible if the type is trivially destructible.");

  static constexpr bool has_header = not base::alloc_traits::is_always_equal::value or store_size_and_capacity;

  struct header_sz_cap {
    [[no_unique_address]] Allocator alloc;
    sz_t size;
    sz_t capacity;
  };

  struct header_no_sz_cap {
    [[no_unique_address]] Allocator alloc;
  };

  using header = std::conditional_t<store_size_and_capacity, header_sz_cap, header_no_sz_cap>;

  static constexpr u64_t Tsz = sizeof(T);
  static constexpr u64_t ptr_size = sizeof(void*);
  static constexpr u64_t header_size = sizeof(header);
  static constexpr u64_t header_alignment = alignof(header);

  // The offset from the first T element, in terms of T.
  // The smallest possible multiple of Tsz that will fit a header.
  static constexpr u64_t header_offset = (not has_header) ? 0 :
    (Tsz >= header_size ? 1 : (header_size + Tsz - 1) / Tsz);

  eden_always_inline [[nodiscard]] static constexpr header* get_header_from(T* data) noexcept requires has_header { assume_assert(data not_eq nullptr); return std::launder((header*)(data - header_offset)); }
  eden_always_inline [[nodiscard]] constexpr header* header_ptr() const noexcept requires has_header { return get_header_from(m_begin); }

  eden_always_inline void
  construct_header() const noexcept
  requires has_header {
    new (header_ptr())
    header(
      std::move(m_alloc),
      this->size(),
      this->capacity());
  }

  constexpr void
  allocate_from_empty(sz_t count)
  noexcept(base::nothrow_allocating) {
    assume_assert(m_begin == nullptr);
    assume_assert(m_size == nullptr);
    assume_assert(m_cap == nullptr);

    m_size = m_begin = m_alloc.allocate(count + header_offset) + header_offset;
    m_cap = m_begin + count;
#ifndef NDEBUG
    if constexpr(has_header) {
      sz_t fake_space = sz_max;
      void* ptr_alignment_check = m_begin;
      assert(std::align(header_alignment, header_size, ptr_alignment_check, fake_space) == m_begin); // ensures the header is propely aligned
    }
#endif
  }

  constexpr void deallocate()
  noexcept(base::nothrow_deallocating) {
    if (m_begin == nullptr) return;
    m_alloc.deallocate(m_begin - header_offset,  this->capacity() + header_offset);
    m_cap = m_size = m_begin = nullptr;
  }

  constexpr void
  expand_to(sz_t count)
  noexcept(base::nothrow_move_construct or base::nothrow_copy_construct) {
    assert(count >= this->size());
    T* new_buff = m_alloc.allocate(count + header_offset) + header_offset;

    auto const sz = size();
    if constexpr(eden_trivially_relocatable(T)) {
      std::memcpy(new_buff, m_begin, sz * sizeof(T));
    } else {
      auto i{0uz};
      while (i not_eq sz) {
        base::alloc_traits::construct(m_alloc, new_buff + i, std::move_if_noexcept(m_begin[i]));
        ++i;
      }
      this->destroy();
    }
    deallocate();
    m_begin = new_buff;
    m_size = m_begin + sz;
    m_cap = m_begin + count;
  }

public:
  using base::size;

  template<releasing_vector_settings other>
  static constexpr bool compatible_settings = is_string == other.is_string and store_size_and_capacity == other.store_size_and_capacity;

  struct released_ptr : owned_ptr<T[]> {
    eden_always_inline constexpr released_ptr() noexcept = default;
    eden_always_inline constexpr explicit released_ptr(T* previously_released_data) noexcept : owned_ptr<T[]>(std::move(previously_released_data)) {}
    eden_always_inline constexpr void destroy_and_deallocate() noexcept(nothrow_deallocating and nothrow_destruct) { releasing_vector::destroy_and_deallocate(std::move(*this)); }

    // note that this method is more expensive than a typical size() call
    eden_always_inline [[nodiscard]] constexpr sz_t size() const noexcept requires store_size_and_capacity { return releasing_vector::data_size(*this); }
  };

  struct released_span : owned_span<T> {
    eden_always_inline constexpr released_span() noexcept = default;
    eden_always_inline constexpr released_span(released_ptr previously_released_data, sz_t sz) noexcept : owned_span<T>(std::move(previously_released_data), sz) {}
    eden_always_inline constexpr released_span(released_ptr&& cstr) noexcept requires is_string : owned_span<T>(std::move(cstr)){}

    eden_always_inline constexpr void destroy_and_deallocate() noexcept(nothrow_deallocating and nothrow_destruct) { releasing_vector::destroy_and_deallocate(std::move(*this)); }
  };

  eden_always_inline constexpr releasing_vector() noexcept(std::is_nothrow_default_constructible_v<Allocator>) = default;
  eden_always_inline constexpr explicit releasing_vector(released_span released_data) noexcept(base::nothrow_move_construct) : releasing_vector(released_ptr(released_data.release())) {}

  template <sz_t N> eden_always_inline constexpr explicit releasing_vector(flags::ReserveInitial<N>) noexcept(nothrow_allocating) { allocate_from_empty(N); }

  constexpr explicit
  releasing_vector(released_ptr released_data)
  noexcept(base::nothrow_move_construct)
  requires store_size_and_capacity {
    assume_assert(m_begin not_eq nullptr);
    m_begin = released_data.release();
    auto h = get_header_pointer_from(m_begin);
    m_alloc = std::move(h->alloc);

    m_size = m_begin + h->size;
    m_cap = m_begin + h->capacity;
    if constexpr (is_string)
      this->pop_back();
    std::destroy_at(h);
  }

  template <sz_t N>
  explicit
  releasing_vector(const char(&c_str)[N])
  noexcept(base::nothrow_allocating)
  requires is_string {
    allocate_from_empty(N);
    std::copy_n(c_str, N - 1, m_begin);
    m_size = m_begin + (N - 1);
  }

  template <releasing_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr releasing_vector(releasing_vector<T, other_settings, other_allocator> &&other) noexcept {
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.m_begin = other.m_size = other.m_cap = nullptr;
  }

  constexpr releasing_vector(releasing_vector const&) = delete;
  constexpr releasing_vector& operator=(releasing_vector const&) = delete;

  eden_always_inline constexpr ~releasing_vector()
  noexcept(base::nothrow_destruct) {
    if (m_begin == nullptr) return;
    this->destroy(); deallocate();
  }

  template <releasing_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr releasing_vector&
  operator=(releasing_vector<T, other_settings, other_allocator> &&other) noexcept {
    this->destroy(); deallocate();
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.m_begin = other.m_size = other.m_cap = nullptr;
    return *this;
  }

  eden_always_inline [[nodiscard]] constexpr T*       data()       noexcept { return m_begin; } // If this is a string, this will NOT return a null terminated string.
  eden_always_inline [[nodiscard]] constexpr T const* data() const noexcept { return m_begin; } // If this is a string, this will NOT return a null terminated string.

  [[nodiscard]] constexpr released_ptr
  release() noexcept
  requires has_header {
    if (m_begin == nullptr) return released_ptr(nullptr);

    if constexpr (is_string) {
      if (this->size() == this->capacity())
        expand_to(this->capacity() + 1);
      this->push_back('\0');
    }

    T* data = m_begin;
    construct_header();
    m_cap = m_size = m_begin = nullptr;
    return released_ptr(data);
  }

  eden_always_inline [[nodiscard]] constexpr released_ptr
  release() noexcept
  requires (not has_header) {
    auto res = m_begin;
    m_cap = m_size = m_begin = nullptr;
    return released_ptr(res);
  }

  eden_always_inline [[nodiscard]] constexpr released_span release_span() noexcept { auto sz = this->size(); return released_span(release(), sz); }

  static constexpr void
  destroy_and_deallocate(released_ptr data)
  noexcept(base::nothrow_deallocating and base::nothrow_destruct)
  requires store_size_and_capacity {
    if (data == nullptr) return;

    auto header_ptr = get_header_from(data.get());
    auto alloc = std::move(header_ptr->alloc);
    if constexpr (not trivially_destructible) {
      auto size = header_ptr->size;
      while (size not_eq 0)
        base::alloc_traits::destroy(alloc, data.get() + --size);
    }
    auto const cap = header_ptr->capacity;
    std::destroy_at(header_ptr);
    alloc.deallocate(data.get() - header_offset, cap + header_offset);
  }

  static constexpr void
  destroy_and_deallocate(released_ptr data)
  noexcept(base::nothrow_deallocating and base::nothrow_destruct)
  requires (not store_size_and_capacity and has_header) {
    if (data == nullptr) return;

    auto const header_ptr = get_header_from(data.get());
    auto const alloc = std::move(header_ptr->alloc);
    auto const cap = header_ptr->capacity;
    std::destroy_at(header_ptr);
    alloc.deallocate(data.get() - header_offset, cap + header_offset);
  }

  eden_always_inline static constexpr void destroy_and_deallocate(released_ptr data, sz_t count) noexcept(base::nothrow_deallocating) requires (not has_header) { Allocator{}.deallocate(data.get(), count); }
  eden_always_inline static constexpr void destroy_and_deallocate(released_span data) noexcept(base::nothrow_deallocating) requires (not has_header) { Allocator{}.deallocate(data.get(), data.size()); }
  eden_always_inline static constexpr void destroy_and_deallocate(released_span data) noexcept(base::nothrow_deallocating and base::nothrow_destruct) { return destroy_and_deallocate(released_ptr(data.get())); }

  static constexpr released_ptr
  copy_data(released_ptr const& data)
  noexcept(base::nothrow_deallocating and base::nothrow_destruct)
  requires (base::copy_constructible and store_size_and_capacity) {
    if (data == nullptr) return released_ptr(nullptr);

    auto header_ptr = get_header_from( (T*)data.get() );
    auto const size = header_ptr->size;
    releasing_vector v(header_ptr->alloc);
    v.reserve(size);

    if constexpr(is_string) {
      for (auto i{0uz}; i<size - 1; ++i)
        v.emplace_back(data[i]);
    }
    else {
      for (auto i{0uz}; i<size; ++i)
        v.emplace_back(data[i]);
    }

    return v.release();
  }

  eden_always_inline static constexpr sz_t
  data_size(released_ptr const& data) noexcept
  requires store_size_and_capacity {
    auto header_ptr = get_header_from( (T*) data.get() );
    return header_ptr->size;
  }

  eden_always_inline static constexpr sz_t
  data_capacity(released_ptr const& data) noexcept
  requires store_size_and_capacity {
    auto header_ptr = get_header_from( (T*) data.get() );
    return header_ptr->capacity;
  }

  eden_always_inline [[nodiscard]] constexpr operator std::string_view()      const noexcept requires is_string { return std::string_view(m_begin, this->size()); }
  eden_always_inline [[nodiscard]] constexpr std::string_view to_stringview() const noexcept requires is_string { return this->operator std::string_view(); }
  eden_always_inline [[nodiscard]] constexpr explicit operator std::string()  const noexcept requires is_string { return std::string(m_begin, this->size()); }
  eden_always_inline [[nodiscard]] constexpr std::string to_stdstring()       const noexcept requires is_string { return this->operator std::string(); }

  template <sz_t N>
  [[nodiscard]] constexpr bool
  operator==( const char(&c_str)[N] ) noexcept
  requires is_string {
    auto const sz = this->size();
    if ((N-1) not_eq sz) return false;

    auto i{0uz};
    while (i < sz) {
      if (m_begin[i] not_eq c_str[i])
        return false;
      ++i;
    }
    assume_assert(c_str[N-1] == '\0' and "Pass a null-terminated string to this function, doofus.");
    return true;
  }

  eden_always_inline [[nodiscard]] constexpr bool operator==(std::string_view   sv)      const noexcept requires is_string { return to_stringview() == sv; }
  eden_always_inline [[nodiscard]] constexpr bool operator==(std::string const& std_str) const noexcept requires is_string { return to_stringview() == std::string_view(std_str); }

};

using releasing_string = releasing_vector<char, releasing_vector_settings<true>{}>;

}