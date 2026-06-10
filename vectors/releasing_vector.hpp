#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../metaprogramming/type_class.hpp"
#include "../owned.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"
#include <format>
#include <memory>
#include <ranges>

namespace eden {
/*
*  ExpansionMult (default 2):
*     -The multiplier applied to the capacity everytime the vector must expand.
*  CString (default false):
*     -Determines whether the vector gains extra string functionality, and will ensure the released string is null-terminated.
*/
template <u64_t ExpansionMult = 2,
          bool CString = false>
requires (ExpansionMult > 1)
struct releasing_vector_settings {
  static constexpr u64_t expansion_mult = ExpansionMult;
  static constexpr bool is_string = CString;
};

template <class T, auto settings = releasing_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires (settings.is_string ? (sizeof(T) == 1 and type<T>::is_integral) : true) and
         (std::allocator_traits<Allocator>::propagate_on_container_move_assignment::value)
class releasing_vector {
  static constexpr bool is_string = settings.is_string;
  static constexpr u64_t expansion_mult = settings.expansion_mult;
  static constexpr bool trivially_destructible = std::is_trivially_destructible_v<T>;

  struct header {
    [[no_unique_address]] Allocator alloc;
    sz_t size;
    sz_t capacity;
  };

  static constexpr u64_t Tsz = sizeof(T);
  static constexpr u64_t ptr_size = sizeof(void*);
  static constexpr u64_t header_size = sizeof(header);
  static constexpr u64_t header_alignment = alignof(header);

  // The offset from the first T element, in terms of T.
  // The smallest possible multiple of Tsz that will fit a header.
  static constexpr u64_t header_offset = (Tsz >= header_size ? 1 : (header_size + Tsz - 1) / Tsz);

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

  [[nodiscard]] static constexpr header*
  get_header_from(T* data) noexcept {
    assume_assert(data not_eq nullptr);
    return std::launder((header*)(data - header_offset));
  }

  [[nodiscard]] constexpr header*
  header_ptr() const noexcept
  { return get_header_from(m_begin); }

  T* m_begin{nullptr};
  T* m_size{nullptr};
  T* m_cap{nullptr};

  constexpr void
  construct_header() const noexcept
  { new (header_ptr()) header(alloc_traits::select_on_container_copy_construction(m_alloc), size(), capacity()); }

  constexpr void allocate_from_empty(sz_t count)
  noexcept(nothrow_allocating) {
    assume_assert(m_begin == nullptr);
    assume_assert(m_size == nullptr);
    assume_assert(m_cap == nullptr);

    m_size = m_begin = m_alloc.allocate(count + header_offset) + header_offset;
    m_cap = m_begin + count;
#ifndef NDEBUG
    sz_t fake_space = sz_max;
    void* ptr_alignment_check = m_begin;
    assert(std::align(header_alignment, header_size, ptr_alignment_check, fake_space) == m_begin); // ensures the header is propely aligned
#endif
  }

  constexpr void deallocate()
  noexcept(nothrow_deallocating) {
    if (m_begin == nullptr)
      return;

    m_alloc.deallocate(m_begin - header_offset,  capacity() + header_offset);
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
    T* new_buff = m_alloc.allocate(count + header_offset) + header_offset;
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

  template<releasing_vector_settings other>
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

  struct released_ptr : public owned_ptr<T[]> {
    constexpr void destroy_and_deallocate()
    noexcept(nothrow_deallocating and nothrow_destruct)
    { releasing_vector::destroy_and_deallocate(std::move(*this)); }

    constexpr released_ptr() noexcept = default;
    explicit constexpr
    released_ptr(T* previously_released_data) noexcept
    : owned_ptr<T[]>(previously_released_data) {}

    // note that this method is more expensive than a typical size() call
    [[nodiscard]] constexpr sz_t
    size() const noexcept
    {return releasing_vector::data_size(*this);}
  };

  struct released_span : public owned_span<T> {
    constexpr void destroy_and_deallocate()
    noexcept(nothrow_deallocating and nothrow_destruct)
    { releasing_vector::destroy_and_deallocate(std::move(*this)); }

    constexpr released_span() noexcept = default;
    constexpr released_span(released_ptr previously_released_data, sz_t sz) noexcept
    : owned_span<T>(std::move(previously_released_data), sz) {}

    constexpr released_span(released_ptr&& cstr) noexcept
    requires is_string : owned_span<T>(std::move(cstr)){}
  };

  constexpr releasing_vector()
  noexcept(std::is_nothrow_default_constructible_v<Allocator>) = default;

  template <sz_t N>
  constexpr explicit
  releasing_vector(flags::ReserveInitial<N>)
  noexcept(nothrow_allocating)
  { allocate_from_empty(N); }

  constexpr explicit
  releasing_vector(released_ptr released_data)
  noexcept(nothrow_move_construct)
  : m_alloc(std::move(get_header_pointer_from(released_data.get())->alloc)),
    m_begin(released_data.release()) {
    assume_assert(m_begin not_eq nullptr);
    auto h = get_header_pointer_from(m_begin);
    m_size = m_begin + h->size;
    m_cap = m_begin + h->capacity;
    if constexpr (is_string)
      pop_back();
    std::destroy_at(h);
  }

  constexpr explicit
  releasing_vector(released_span released_data)
  noexcept(nothrow_move_construct)
  : releasing_vector(released_ptr(released_data.release())) {}

  template <sz_t N>
  constexpr explicit
  releasing_vector(const char(&c_str)[N])
  noexcept(nothrow_allocating)
  requires is_string {
    allocate_from_empty(N);
    std::copy_n(c_str, N - 1, m_begin);
    m_size = m_begin + (N - 1);
  }

  constexpr explicit
  releasing_vector(const Allocator &alloc) noexcept
  : m_alloc(alloc) {}

  constexpr explicit
  releasing_vector(sz_t count, const Allocator &alloc = Allocator())
  noexcept(noexcept(allocate_and_construct(count)))
  : m_alloc(alloc) { allocate_and_construct(count); }

  constexpr releasing_vector(sz_t count, const T& value, const Allocator &alloc = Allocator())
  noexcept(nothrow_copy_construct)
  requires copy_constructible
  : m_alloc(alloc) { allocate_and_construct(count, std::forward<T>(value)); }

  constexpr releasing_vector(const releasing_vector&) = delete;
  constexpr releasing_vector& operator=(const releasing_vector&) = delete;

  template <releasing_vector_settings other_settings, allocator_for_c<T> other_allocator>
  requires compatible_settings<other_settings> and same_c<Allocator, other_allocator>
  constexpr releasing_vector(releasing_vector<T, other_settings, other_allocator> &&other) noexcept
  : m_alloc(std::move(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  { other.m_begin = other.m_size = other.m_cap = nullptr; }

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
  noexcept(nothrow_destruct and nothrow_deallocating) {
    if (m_begin == nullptr)
      return;

    destroy();
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
  at(sz_t idx) {
    if (m_begin + idx >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::releasing_vector with size of {}.", idx, size()));
    return m_begin[idx];
  }

  [[nodiscard]] constexpr const T&
  at(sz_t idx) const {
    if (m_begin + idx >= m_size)
      throw std::out_of_range(std::format("Element access at index {} in eden::releasing_vector with size of {}.", idx, size()));
    return m_begin[idx];
  }

  [[nodiscard]] constexpr T&
  operator[](sz_t idx) noexcept {
    assume_assert(m_begin); assert(idx < size());
    return m_begin[idx];
  }

  [[nodiscard]] constexpr const T&
  operator[](sz_t idx) const noexcept {
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

  // If this is a string, this will NOT return a null terminated string.
  [[nodiscard]] constexpr T*
  data() noexcept { return m_begin; }

  // If this is a string, this will NOT return a null terminated string.
  [[nodiscard]] constexpr const T*
  data() const noexcept { return m_begin; }

  [[nodiscard]] constexpr released_ptr
  release() noexcept {
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

  [[nodiscard]] constexpr released_span
  release_span() noexcept {
    auto sz = size();
    return released_span(release(), sz);
  }

  static constexpr void
  destroy_and_deallocate(released_ptr data)
  noexcept(nothrow_deallocating and nothrow_destruct) {
    if (data == nullptr)
      return;

    auto header_ptr = get_header_from(data.get());
    auto alloc = std::move(header_ptr->alloc);
    if constexpr (not trivially_destructible) {
      auto size = header_ptr->size;
      while (size not_eq 0)
        alloc_traits::destroy(alloc, data.get() + --size);
    }
    auto const cap = header_ptr->capacity;
    std::destroy_at(header_ptr);
    alloc.deallocate(data.get() - header_offset, cap + header_offset);
  }

  static constexpr void
  destroy_and_deallocate(released_span data)
  noexcept(nothrow_deallocating and nothrow_destruct)
  { return destroy_and_deallocate(released_ptr(data.get())); }

  // investigate possible bug involving string specialization
  // this might be adding a redundant null terminator
  static constexpr released_ptr
  copy_data(const released_ptr& data)
  noexcept(nothrow_deallocating and nothrow_destruct)
  requires copy_constructible {
    if (data == nullptr)
      return released_ptr(nullptr);

    auto header_ptr = get_header_from(const_cast<T*>(data.get()));
    auto const size = header_ptr->size;
    releasing_vector v(header_ptr->alloc);
    v.reserve(size);

    for (auto i{0uz}; i<size; ++i)
      v.emplace_back(data[i]);
    return v.release();
  }

  static constexpr sz_t
  data_size(const released_ptr& data) noexcept {
    auto header_ptr = get_header_from(const_cast<T*>(data.get()));
    return header_ptr->size;
  }

  static constexpr sz_t
  data_capacity(const released_ptr& data) noexcept {
    auto header_ptr = get_header_from(const_cast<T*>(data.get()));
    return header_ptr->capacity;
  }

  [[nodiscard]] constexpr explicit
  operator std::span<T>() const noexcept
  { return std::span(m_begin, size()); }

  [[nodiscard]] constexpr std::span<T>
  to_span() const noexcept
  { return (*this).operator std::span<T>(); }

  [[nodiscard]] constexpr
  operator std::string_view() const noexcept
  requires is_string
  { return std::string_view(m_begin, size()); }

  [[nodiscard]] constexpr std::string_view
  to_stringview() const noexcept
  requires is_string
  { return (*this).operator std::string_view(); }

  [[nodiscard]] constexpr explicit
  operator std::string() const noexcept
  requires is_string
  { return std::string(m_begin, size()); }

  [[nodiscard]] constexpr std::string
  to_stdstring() const noexcept
  requires is_string
  { return (*this).operator std::string(); }

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
  operator==(const std::string& std_str) const noexcept
  requires is_string
  { return to_stringview() == std::string_view(std_str); }

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
};

using releasing_string = releasing_vector<char, releasing_vector_settings<2, true>{}>;

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