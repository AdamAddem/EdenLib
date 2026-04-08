#pragma once
#include "assume_assert.hpp"
#include "concepts.hpp"
#include "type_flags.hpp"
#include "typedefs.hpp"
#include <format>
#include <memory>
#include <ranges>

namespace eden {
/*
*  ExpansionMult (default 2):
*    -The multiplier applied to the capacity everytime it the vector must expand.
*  StoreHeader (default true):
*    -Determines whether the vector allocates a pointer to a header before the data.
*    -If true, max(8, sizeof(T)), bytes extra are allocated and stored. Can be inefficient for small arrays and/or small types.
*    -If true, deallocation just requires the released pointer.
*    -If false, the user will be handed a header upon release that will need to be stored.
*  Allocator (default std::allocator<T>):
*    -The allocator for the specified type.
*/
template <
          bool StoreHeader = true,
          u64_t ExpansionMult = 2,
          bool CString = false>
requires (ExpansionMult > 1)
struct releasing_vector_settings {
  using Noheader = releasing_vector_settings<false>;

  static constexpr u64_t expansion_mult = ExpansionMult;
  static constexpr bool store_header = StoreHeader;
  static constexpr bool is_string = CString;
};

template <class T, releasing_vector_settings settings = releasing_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (settings.is_string ? (sizeof(T) == 1 and type<T>::is_integral) : true) and
         (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value)
class releasing_vector {
  static constexpr bool is_string = decltype(settings)::is_string;
  static constexpr sz_t expansion_mult = decltype(settings)::expansion_mult;
  static constexpr bool store_header = decltype(settings)::store_header;
  static constexpr bool trivially_destructible = std::is_trivially_destructible_v<T>;

  struct header {
    [[no_unique_address]] Allocator alloc;
    sz_t size;
    sz_t capacity;
  };

  static constexpr sz_t Tsz = sizeof(T);
  static constexpr sz_t ptr_size = sizeof(void*);
  static constexpr sz_t header_size = not store_header ? 0 : sizeof(header);

  //The offset from the first T element, in terms of T. Zero if not storing a header.
  //(Assuming a zero sized allocator and 8 byte pointer size)
  //Ex: if T is u16_t, header_count is 4 (the pointer occupies the space of 4 u16's)
  //    if T is u128_t, header_count is 1 (the vector has to allocate in multiples of T, Tsize is 16 and pointer is 8, so 8 bytes are wasted)
  static constexpr sz_t header_count =
    not store_header ? 0 : (
    Tsz >= ptr_size ? 1 : (ptr_size + Tsz - 1) / Tsz);

  static constexpr bool nothrow_copy_construct = std::is_nothrow_copy_constructible_v<T>;

  [[no_unique_address]] Allocator m_alloc;
  using alloc_traits = std::allocator_traits<Allocator>;
  static constexpr bool stateless_allocator = std::allocator_traits<Allocator>::is_always_equal;
  static constexpr bool nothrow_destruct = noexcept(alloc_traits::destroy(m_alloc, static_cast<T*>(0)));
  static constexpr bool nothrow_allocating = noexcept(m_alloc.allocate(1));
  static constexpr bool nothrow_deallocating = noexcept(m_alloc.deallocate(static_cast<T*>(0), 1));

  [[nodiscard]] static constexpr header*
  get_header_pointer_from(T* data) noexcept
  requires store_header {
    assume_assert(data not_eq nullptr);
                         //https://wiki.c2.com/?ThreeStarProgrammer/
    return *std::launder(reinterpret_cast<header**>(data - header_count));
  }

  [[nodiscard]] constexpr header*
  header_pointer() const noexcept
  requires store_header
  {return get_header_pointer_from(m_begin);}

  [[nodiscard]] static constexpr header**
  get_header_ptr_location_from(T* data) noexcept
  requires store_header {
    assume_assert(data not_eq nullptr);
    return std::launder(reinterpret_cast<header**>(data - header_count));
  }

  [[nodiscard]] constexpr header**
  header_pointer_location() const noexcept
  requires store_header
  {return get_header_ptr_location_from(m_begin);}

  T* m_begin{nullptr};
  T* m_size{nullptr};
  T* m_cap{nullptr};

  constexpr void
  construct_header() const noexcept
  requires store_header {
    new (header_pointer()) header(alloc_traits::select_on_container_copy_construction(m_alloc), size(), capacity());
  }

  constexpr void allocate(sz_t count)
  noexcept(nothrow_allocating) {
    assume_assert(m_begin == nullptr);
    assume_assert(m_size == nullptr);
    assume_assert(m_cap == nullptr);

    m_size = m_begin = m_alloc.allocate(count + header_count) + header_count;
    m_cap = m_begin + count;
  }

  constexpr void first_allocation(sz_t count)
  noexcept(nothrow_allocating) {
    allocate(count);
    if constexpr (store_header)
      *header_pointer_location() = static_cast<header*>(::operator new(sizeof(header), static_cast<std::align_val_t>(alignof(header))));
  }

  constexpr void deallocate()
  noexcept(nothrow_deallocating) {
    if (m_begin == nullptr)
      return;

    m_alloc.deallocate(m_begin - header_count,  capacity() + header_count);
    m_cap = m_size = m_begin = nullptr;
  }

  constexpr void destroy()
  noexcept(nothrow_destruct) {
    if (m_begin == nullptr)
      return;
    while (m_size not_eq m_begin)
      alloc_traits::destroy(m_alloc, --m_size);
  }

  template <class ...Args>
  constexpr void allocate_and_construct(sz_t count, Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    first_allocation(count);
    while (m_size not_eq m_cap)
      alloc_traits::construct(m_alloc, m_size++, std::forward<Args>(args)...);
  }

  constexpr void expand_to(sz_t count)
  noexcept(std::is_nothrow_copy_constructible_v<T>) {
    assert(count >= size());
    T* new_buff = m_alloc.allocate(count + header_count) + header_count;
    auto i{0uz};
    while ((m_begin + i) not_eq m_size) {
      alloc_traits::construct(m_alloc, new_buff + i, std::move_if_noexcept(*(m_begin + i)));
      ++i;
    }
    if constexpr (store_header)
      *std::launder(reinterpret_cast<void**>(new_buff - header_count)) = header_pointer();

    destroy(); deallocate();
    m_begin = new_buff;
    m_size = m_begin + i;
    m_cap = m_begin + count;
  }

public:

  template<releasing_vector_settings second>
  static constexpr bool compatible_settings = store_header == second.store_header;

  struct const_iterator {
    using iterator_category = std::contiguous_iterator_tag;
    using value_type        = std::remove_cv_t<T>;
    using element_type      = value_type;

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
    {const_iterator tmp = *this; ++m_ptr; return tmp;}

    [[nodiscard]] constexpr const_iterator&
    operator--() noexcept
    {--m_ptr; return *this;}

    [[nodiscard]] constexpr const_iterator
    operator--(int) noexcept
    {const_iterator tmp = *this; --m_ptr; return tmp;}

    [[nodiscard]] constexpr const_iterator&
    operator+=(sz_t n) noexcept
    {m_ptr += n; return *this;}

    [[nodiscard]] constexpr const_iterator&
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
    {iterator tmp = *this; ++m_ptr; return tmp;}

    [[nodiscard]] constexpr iterator&
    operator--() noexcept
    {--m_ptr; return *this;}

    [[nodiscard]] constexpr iterator
    operator--(int) noexcept
    {iterator tmp = *this; --m_ptr; return tmp;}

    [[nodiscard]] constexpr iterator&
    operator+=(sz_t n) noexcept
    {m_ptr += n; return *this;}

    [[nodiscard]] constexpr iterator&
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

  class data_handle {
    friend class releasing_vector;

    [[no_unique_address]] Allocator alloc;
    sz_t size;
    sz_t capacity;
  public:
    owned_ptr<T[]> data;

    constexpr ~data_handle() {
      if (data == nullptr)
        return;

      if constexpr (not trivially_destructible) {
        while (size not_eq 0)
          alloc_traits::destroy(alloc, data + --size);
      }

      alloc.deallocate(data, capacity);
    }
  };

  using released_ptr = owned_ptr<T[]>;

  constexpr releasing_vector() = default;

  template <sz_t N>
  constexpr explicit
  releasing_vector(flags::ReserveInitial<N>)
  noexcept(nothrow_allocating)
  {first_allocation(N);}

  constexpr explicit
  releasing_vector(released_ptr released_data)
  noexcept(type<Allocator>::nothrow_move_constructible)
  requires store_header
  : m_alloc(std::move(get_header_pointer_from(released_data.get())->alloc)),
    m_begin(released_data.release()) {
    assume_assert(m_begin not_eq nullptr);
    auto h = get_header_pointer_from(m_begin);
    m_size = m_begin + h->size;
    m_cap = m_begin + h->capacity;
    if constexpr (is_string)
      pop_back();
    std::destroy(h);
  }

  template <sz_t N>
  constexpr explicit
  releasing_vector(const char(&c_str)[N])
  noexcept(nothrow_allocating)
  requires is_string {
    first_allocation(N);
    std::copy_n(c_str, N - 1, m_begin);
    m_size = m_begin + (N - 1);
  }

  constexpr explicit
  releasing_vector(const Allocator &alloc) noexcept
  : m_alloc(alloc) {}

  constexpr explicit
  releasing_vector(sz_t count, const Allocator &alloc = Allocator()) noexcept
  : m_alloc(alloc) {allocate_and_construct(count);}

  constexpr releasing_vector(sz_t count, const T& value, const Allocator &alloc = Allocator())
  noexcept(nothrow_copy_construct)
  requires std::is_copy_constructible_v<T>
  : m_alloc(alloc) {allocate_and_construct(count, std::forward<T>(value));}

  constexpr releasing_vector(const releasing_vector &other) = delete;
  constexpr releasing_vector& operator=(const releasing_vector &other) = delete;

  template <releasing_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr releasing_vector(releasing_vector<T, other_settings, other_allocator> &&other) noexcept
  : m_alloc(std::move(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  {other.m_begin = other.m_size = other.m_cap = nullptr;}

  template <releasing_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr void swap(releasing_vector<T, other_settings, other_allocator>& other) noexcept {
    if constexpr (alloc_traits::propagate_on_container_swap)
      std::swap(m_alloc, other.m_alloc);
    else
      assume_assert(m_alloc not_eq other.m_alloc and "Undefined behavior if this triggers.");

    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  constexpr ~releasing_vector()
  noexcept(noexcept(destroy()) and noexcept(deallocate())) {
    if (m_begin == nullptr)
      return;

    destroy();
    if constexpr (store_header) {
      auto header_ptr = header_pointer();
      ::operator delete(header_ptr, static_cast<std::align_val_t>(alignof(header)));
    }
    deallocate();
  }

  template <releasing_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr releasing_vector&
  operator=(releasing_vector<T, other_settings, other_allocator> &&other) noexcept {
    destroy(); deallocate();
    m_alloc = std::move(other.m_alloc);
    m_begin = other.m_begin; m_size = other.m_size; m_cap = other.m_cap;
    other.m_begin = other.m_size = other.m_cap = nullptr;
    return *this;
  }

  [[nodiscard]] constexpr T&
  at(sz_t pos) {
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} eden::releasing_vector with size of {}.", pos, size()));
    return *(m_begin + pos);
  }

  [[nodiscard]] constexpr const T&
  at(sz_t pos) const {
    if (m_begin + pos >= m_size)
      throw std::out_of_range(std::format("Element access at index {} eden::releasing_vector with size of {}.", pos, size()));
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
  {assume_assert(m_size); return *(m_size - 1);}

  [[nodiscard]] constexpr const T&
  back() const noexcept
  {assume_assert(m_size); return *(m_size - 1);}

  [[nodiscard]] constexpr released_ptr
  release() noexcept
  requires store_header {
    if (m_begin == nullptr)
      return released_ptr(nullptr);

    if constexpr (is_string) {
      if (size() == capacity())
        expand_to(capacity() + 1);
      push_back('\0');
    }

    T* data = m_begin;
    construct_header();
    m_cap = m_size = m_begin = nullptr;
    return released_ptr(data);
  }

  [[nodiscard]] constexpr data_handle
  release() noexcept
  requires (not store_header) {
    if (m_begin == nullptr)
      return {alloc_traits::select_on_container_copy_construction(m_alloc), 0, 0, nullptr};

    if constexpr (is_string) {
      if (size() == capacity())
        expand_to(capacity() + 1);
      push_back('\0');
    }

    T* data = m_begin;
    sz_t sz = size();
    sz_t cap = capacity();
    m_cap = m_size = m_begin = nullptr;

    return {alloc_traits::select_on_container_copy_construction(m_alloc), sz, cap, data};
  }

  static constexpr void
  destroy_and_deallocate(released_ptr data)
  noexcept(nothrow_deallocating and nothrow_destruct)
  requires store_header {
    if (data == nullptr)
      return;

    auto header_ptr = get_header_pointer_from(data.get());
    auto& alloc = header_ptr->alloc;
    if constexpr (not trivially_destructible) {
      sz_t size = header_ptr->size;
      while (size not_eq 0)
        alloc_traits::destroy(alloc, data.get() + --size);
    }
    const sz_t cap = header_ptr->capacity;
    alloc.deallocate(data.get() - header_count, cap + header_count);

    std::destroy_at(header_ptr);
    ::operator delete(header_ptr, static_cast<std::align_val_t>(alignof(header)));
  }

  [[nodiscard]] constexpr explicit
  operator std::span<T>() const noexcept
  {return std::span(m_begin, size());}

  [[nodiscard]] constexpr std::span<T>
  to_span() const noexcept
  {return static_cast<std::span<T>>(*this);}

  [[nodiscard]] constexpr
  operator std::string_view() const noexcept
  requires is_string
  {return std::string_view(m_begin, size());}

  [[nodiscard]] constexpr std::string_view
  to_stringview() const noexcept
  requires is_string
  {return static_cast<std::string_view>(*this);}

  [[nodiscard]] constexpr explicit
  operator std::string() const noexcept
  requires is_string
  {return std::string(m_begin, size());}

  [[nodiscard]] constexpr std::string
  to_stdstring() const noexcept
  requires is_string
  {return static_cast<std::string>(*this);}

  template <sz_t N>
  [[nodiscard]] constexpr bool
  operator==(const char(&c_str)[N]) noexcept
  requires is_string {
    const sz_t sz = size();
    if ((N-1) not_eq sz)
      return false;

    auto i{0uz};
    while (i < sz) {
      if (m_begin[i] not_eq c_str[i])
        return false;
      ++i;
    }
    assume_assert(c_str[N-1] == '\0' and "Pass a terminated string to this function, doofus");
    return true;
  }

  [[nodiscard]] constexpr bool
  operator==(const std::string& std_str) noexcept
  requires is_string {
    const sz_t sz = size();
    if (std_str.size() not_eq sz)
      return false;

    auto i{0uz};
    while (i < sz) {
      if (m_begin[i] not_eq std_str[i])
        return false;
      ++i;
    }
    return true;
  }

  [[nodiscard]] constexpr bool
  empty() const noexcept
  {return m_size == m_begin;}

  [[nodiscard]] constexpr sz_t
  size() const noexcept
  {return m_size - m_begin;}

  constexpr void reserve(sz_t new_capacity) const noexcept {
    if (m_begin == nullptr)
      first_allocation(new_capacity);
    else if (capacity() < new_capacity)
      expand_to(new_capacity);
  }

  constexpr void resize(sz_t count) noexcept
  requires std::is_default_constructible_v<T> {
    if (m_begin == nullptr)
      return allocate_and_construct(count);

    const sz_t current_size = size();
    if (current_size == count)
      return;
    if (current_size > count)
      return expand_to(count);

    if (capacity() < count)
      expand_to(count);

    const auto num_default{count - size()};
    auto i{0uz};
    while (i < num_default)
      alloc_traits::construct(m_alloc, m_size + i++);
  }

  constexpr void resize(sz_t count, const T& value) noexcept
  requires std::is_copy_constructible_v<T> {
    if (m_begin == nullptr)
      return allocate_and_construct(count, value);

    const sz_t current_size = size();
    if (current_size == count)
      return;
    if (current_size > count)
      return expand_to(count);

    if (capacity() < count)
      expand_to(count);

    const auto num_default{count - size()};
    auto i{0uz};
    while (i < num_default)
      alloc_traits::construct(m_alloc, m_size + i++, value);
  }

  [[nodiscard]] constexpr sz_t
  capacity() const noexcept
  {return m_cap - m_begin;}

  constexpr void shrink_to_fit() const noexcept {
    if (size() not_eq capacity())
      expand_to(size());
  }

  constexpr void clear() noexcept {destroy();}

  template <class... Args> constexpr T&
  emplace_back(Args&&... args)
  noexcept(std::is_nothrow_constructible_v<T, Args...>)
  requires std::is_constructible_v<T, Args...> {
    if (m_begin == nullptr)
      first_allocation(1);
    else if (m_size == m_cap)
      expand_to(capacity() * expansion_mult);

    alloc_traits::construct(m_alloc, m_size, std::forward<Args>(args)...);
    return *(m_size++);
  }

  constexpr void push_back(const T& value)
  noexcept(nothrow_copy_construct) {emplace_back(value);}

  constexpr void push_back(T&& value)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  requires std::is_move_constructible_v<T> {emplace_back(std::forward<T>(value));}

  constexpr void pop_back()
  noexcept(nothrow_destruct)
  {assume_assert(m_begin); assume_assert(m_size not_eq m_begin); alloc_traits::destroy(m_alloc, m_size--);}
};

using releasing_string = releasing_vector<char, releasing_vector_settings<true, 2, true>{}>;

template <class T>
using noheader_releasing_vector = releasing_vector<T, releasing_vector_settings<true, 2, true>{}>;

template <class T, releasing_vector_settings lhs_settings, releasing_vector_settings rhs_settings, allocator_for_c<T> allocator>
requires releasing_vector<T, lhs_settings, allocator>::template compatible_settings<rhs_settings>
[[nodiscard]] constexpr bool
operator==(const releasing_vector<T, lhs_settings, allocator>& lhs, const releasing_vector<T, rhs_settings, allocator>& rhs) noexcept
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