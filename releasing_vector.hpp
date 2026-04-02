#pragma once
#include "assume_assert.hpp"
#include "concepts.hpp"
#include "type_flags.hpp"
#include "typedefs.hpp"
#include <format>
#include <memory>
#include <ranges>

namespace eden {


/*  idx 0      idx 1...
 *  [sz, cap], [...]
 *
 *
 */

 /*
 *  InitialCapacity (default 0):
 *    -The initial capacity for the vector, equivalent to calling .reserve() immediately after construction.
 *  ExpansionMult (default 2):
 *    -The multiplier applied to the capacity everytime the capacity fills.
 *  StoreHeader (default true):
 *    -Determines whether the vector allocates a pointer to a header before the data.
 *    -If true, max(8, sizeof(T)) bytes extra are allocated and stored. Can be inefficient for small arrays and small types.
 *    -If true, deallocation just requires the released pointer.
 *    -If false, the user will be handed a header upon release that will need to be stored.
 *  Allocator (default std::allocator<T>):
 *    -The allocator for the specified type.
 */
template <class T,
          u64_t InitialCapacity = 0,
          u64_t ExpansionMult = 2,
          bool StoreHeader = true,
          allocator_for_c<T> Allocator = std::allocator<T>>
requires (ExpansionMult > 1)
struct releasing_vector_settings {
  static constexpr u64_t initial_capacity = InitialCapacity;
  static constexpr u64_t expansion_mult = ExpansionMult;
  static constexpr bool store_header = StoreHeader;
  using allocator = Allocator;
};

template<class T>
using default_releasing_vector = releasing_vector_settings<T>;

template<class T>
using noheader_releasing_vector = releasing_vector_settings<T, 0, 2, false>;

template<releasing_vector_settings first, releasing_vector_settings second>
concept compatible_releasing_vector_settings =
  std::is_same_v<typename decltype(first)::allocator, typename decltype(second)::allocator> and decltype(first)::store_header == decltype(second)::store_header;

template <class T, releasing_vector_settings settings = default_releasing_vector<T>{}>
class releasing_vector {

  static constexpr sz_t initial_capacity = decltype(settings)::initial_capacity;
  static constexpr sz_t expansion_mult = decltype(settings)::expansion_mult;
  static constexpr bool store_header = decltype(settings)::store_header;
  using allocator = decltype(settings)::allocator;
  static constexpr bool trivially_destructible = std::is_trivially_destructible_v<T>;

  struct header {
    [[no_unique_address]] allocator alloc;
    sz_t info[trivially_destructible ? 1 : 2];

    static constexpr u64_t size = 0;
    static constexpr u64_t capacity = trivially_destructible ? 0 : 1;
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

  static constexpr bool is_string = same_c<T, char>;
  static constexpr bool nothrow_copy_construct = std::is_nothrow_copy_constructible_v<T>;

  [[no_unique_address]] allocator m_alloc;
  using alloc_traits = std::allocator_traits<allocator>;
  static constexpr bool stateless_allocator = std::allocator_traits<allocator>::is_always_equal;
  static constexpr bool nothrow_destruct = noexcept(alloc_traits::destroy(m_alloc, static_cast<T*>(0)));
  static constexpr bool nothrow_allocating = noexcept(m_alloc.allocate(1));
  static constexpr bool nothrow_deallocating = noexcept(m_alloc.deallocate(static_cast<T*>(0), 1));


  [[nodiscard]] static constexpr header*
  get_header_pointer_from(T* data) noexcept
  requires store_header {
    assert(data);
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
    assert(data);
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
    if constexpr (trivially_destructible)
      new (header_pointer()) header(alloc_traits::select_on_container_copy_construction(m_alloc), {capacity()});
    else
      new (header_pointer()) header(alloc_traits::select_on_container_copy_construction(m_alloc), {size(), capacity()});
  }

  constexpr void allocate(sz_t count)
  noexcept(nothrow_allocating) {
    assert(m_begin == nullptr);
    assert(m_size == nullptr);
    assert(m_cap == nullptr);

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

    [[no_unique_address]] allocator alloc;
    sz_t info[trivially_destructible ? 1 : 2];
    static constexpr u64_t size = 0;
    static constexpr u64_t capacity = trivially_destructible ? 0 : 1;
  public:
    owned_ptr<T> data;

    constexpr ~data_handle() {
      if (data == nullptr)
        return;

      if constexpr (not trivially_destructible) {
        sz_t& sz = info[size];
        while (sz not_eq 0)
          alloc_traits::destroy(alloc, data + --sz);
      }
      sz_t& cap = info[capacity];
      alloc.deallocate(data, cap);
    }
  };

  constexpr releasing_vector()
  noexcept(nothrow_allocating) {
    if constexpr (initial_capacity > 0)
      first_allocation(initial_capacity);
  }

  explicit constexpr releasing_vector(const allocator &alloc) noexcept
  : m_alloc(alloc_traits::select_on_container_copy_construction(alloc)) {}

  explicit releasing_vector(sz_t count, const allocator &alloc = allocator()) noexcept
  : m_alloc(alloc_traits::select_on_container_copy_construction(alloc))
  {allocate_and_construct(count);}

  constexpr releasing_vector(sz_t count, const T &value, const allocator &alloc = allocator())
  noexcept(nothrow_copy_construct)
  requires std::is_copy_constructible_v<T>
  : m_alloc(alloc_traits::select_on_container_copy_construction(alloc))
  {allocate_and_construct(count, std::forward<T>(value));}

  constexpr releasing_vector(const releasing_vector &other) = delete;
  constexpr releasing_vector& operator=(const releasing_vector &other) = delete;

  template <releasing_vector_settings other_settings>
  requires compatible_releasing_vector_settings<settings, other_settings>
  constexpr releasing_vector(releasing_vector<T, other_settings> &&other) noexcept
  : m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc)), m_begin(other.m_begin), m_size(other.m_size), m_cap(other.m_cap)
  {other.m_begin = other.m_size = other.m_cap = nullptr;}

  template <releasing_vector_settings other_settings>
  requires compatible_releasing_vector_settings<settings, other_settings>
  constexpr void swap(releasing_vector<T, other_settings>& other) noexcept {
    std::swap(m_alloc, other.m_alloc);
    std::swap(m_begin, other.m_begin); std::swap(m_size, other.m_size);
    std::swap(m_cap, other.m_cap); std::swap(m_alloc, other.m_alloc);
  }

  constexpr ~releasing_vector()
  noexcept(noexcept(destroy()) and noexcept(deallocate()))
  {destroy(); deallocate();}

  template <releasing_vector_settings other_settings>
  requires compatible_releasing_vector_settings<settings, other_settings>
  constexpr releasing_vector&
  operator=(releasing_vector<T, other_settings> &&other) noexcept {
    destroy(); deallocate();
    m_alloc = alloc_traits::select_on_container_copy_construction(other.m_alloc);
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

  [[nodiscard]] constexpr T*
  data() const noexcept
  {return m_begin;}

  [[nodiscard]] constexpr T*
  release() noexcept
  requires store_header {
    if (m_begin == nullptr)
      return nullptr;
    T* data = m_begin;
    construct_header();
    m_cap = m_size = m_begin = nullptr;
    return data;
  }

  [[nodiscard]] constexpr data_handle
  release() noexcept
  requires (not store_header) {
    if (m_begin == nullptr)
      return {alloc_traits::select_on_container_copy_construction(m_alloc), {0,0}, nullptr};
    T* data = m_begin;

    sz_t sz;
    sz_t cap = capacity();
    m_cap = m_size = m_begin = nullptr;

    if constexpr(not trivially_destructible) {
      sz = size();
      return {alloc_traits::select_on_container_copy_construction(m_alloc), {sz, cap}, data};
    } else {
      return {alloc_traits::select_on_container_copy_construction(m_alloc), {cap}, data};
    }
  }

  static constexpr void
  destroy_and_deallocate(T* data)
  noexcept(nothrow_deallocating and nothrow_destruct)
  requires store_header {
    if (data == nullptr)
      return;

    auto header_ptr = get_header_pointer_from(data);
    auto& alloc = header_ptr->alloc;
    if constexpr (not trivially_destructible) {
      sz_t size = header_ptr->info[header::size];
      while (size not_eq 0)
        alloc_traits::destroy(alloc, data + --size);
    }
    const sz_t cap = header_ptr->info[header::capacity];
    alloc.deallocate(data - header_count, cap + header_count);

    std::destroy_at(header_ptr);
    ::operator delete(header_ptr, static_cast<std::align_val_t>(alignof(header)));
  }

  [[nodiscard]] constexpr explicit
  operator std::span<T>() const noexcept
  {return std::span(m_begin, size());}

  [[nodiscard]] constexpr std::span<T>
  to_span() const noexcept
  {return static_cast<std::span<T>>(*this);}

  [[nodiscard]] constexpr explicit
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

  constexpr void clear() noexcept
  {destroy();}

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

  constexpr void push_back(const T &value)
  noexcept(nothrow_copy_construct)
  {emplace_back(value);}

  constexpr void push_back(T &&value)
  noexcept(std::is_nothrow_move_constructible_v<T>)
  requires std::is_move_constructible_v<T>
  {emplace_back(std::forward<T>(value));}

  constexpr void pop_back()
  noexcept(nothrow_destruct)
  {assume_assert(m_begin); assume_assert(m_size not_eq m_begin); alloc_traits::destroy(m_alloc, m_size--);}
};

using releasing_string = releasing_vector<char>;



template <class T, releasing_vector_settings a_settings, releasing_vector_settings b_settings>
requires compatible_releasing_vector_settings<a_settings, b_settings>
constexpr void
swap(releasing_vector<T, a_settings>& a, releasing_vector<T, b_settings>& b) noexcept
{a.swap(b);}

template <class T, releasing_vector_settings lhs_settings, releasing_vector_settings rhs_settings>
requires compatible_releasing_vector_settings<lhs_settings, rhs_settings>
[[nodiscard]] constexpr bool
operator==(const releasing_vector<T, lhs_settings>& lhs, const releasing_vector<T, rhs_settings>& rhs) noexcept
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