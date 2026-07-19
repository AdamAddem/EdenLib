#pragma once
#include "../typedefs.hpp"
#include "../metaprogramming/packs.hpp"
#include "../type_flags.hpp"

#include <cstring>
#include <format>
#include <exception>

namespace eden {

#define launder_cast(Type, ptr) std::launder( (Type*) (ptr) )
#define launder_castT(ptr) std::launder( (T*) (ptr) )

namespace detail {

template <class First, class... Rest>
eden_always_inline static constexpr void
destroy_all(byte_t** begins, sz_t size, sz_t const* idx_arr) {
  if constexpr(sizeof...(Rest)) destroy_all<Rest...>(begins, size, idx_arr + 1);
  First* const first_begin = (First*)begins[idx_arr[0]];
  First* end = launder_cast(First, first_begin + size);
  while(end not_eq first_begin)
    std::destroy_at(--end);
}

template <class First, class... Rest>
eden_always_inline static constexpr void
move_construct_slices(byte_t** old_begins, byte_t* new_begin, sz_t size_each, sz_t const* idx_arr, sz_t new_capacity_each) {
  auto const my_idx = idx_arr[0];
  First* first_begin = launder_cast(First, old_begins[my_idx]);
  First* construct_location = (First*)new_begin;

  for(auto i{0uz}; i<size_each; ++i)
    std::construct_at( construct_location + i, std::move_if_noexcept(first_begin[i]) );
  old_begins[my_idx] = new_begin;

  if constexpr(sizeof...(Rest)) {
    new_begin += new_capacity_each * sizeof(First); // does not account for alignment, fix
    move_construct_slices<Rest...>(old_begins, new_begin, size_each, idx_arr + 1, new_capacity_each);
  }
}

template <class First, class... Rest>
eden_always_inline static constexpr void
add_to_end(byte_t** begins, sz_t size_each, sz_t const* idx_arr, auto&& first_args, auto&&... rest_args) {
  First* open_slot = ( (First*)begins[idx_arr[0]] ) + size_each;

  std::apply(
    [open_slot]<class... Args>(Args&&... args){
      std::construct_at(
        open_slot,
        std::forward<Args>(args)...
      );
    },
    std::forward<decltype(first_args)>(first_args)
  );

  if constexpr(sizeof...(Rest))
    add_to_end<Rest...>(begins, size_each, idx_arr + 1, std::forward<decltype(rest_args)>(rest_args)...);
}

}

// TODO: Add settings and custom allocator support
template <class... Ts>
requires (sizeof...(Ts) > 1)
class contiguous_soa {
  static constexpr auto NumTs = sizeof...(Ts);
  static constexpr auto ExpansionMult = 2;
  static constexpr SizePack<Ts...> TSizesBytes{};
  static constexpr AlignPack<Ts...> TAlignmentsBytes{};
  static constexpr auto FirstAllocCapacity = 1;
  static constexpr auto buffer_begin_idx = TAlignmentsBytes.map_to_idx[0];

  eden_always_inline [[nodiscard]] static constexpr sz_t map_idx(sz_t raw_idx) { return TAlignmentsBytes.map_to_idx[raw_idx]; }
  template <class T>
  requires is_one_of<T, Ts...>
  static constexpr sz_t mapped_idx = map_idx( idx_in_pack<T, Ts...> );

  byte_t* begins[NumTs]{};
  sz_t    size_each{};
  sz_t    capacity_each{};
  sz_t    buffer_size_bytes{};

  eden_always_inline [[nodiscard]] static byte_t* alloc_new_buffer(sz_t num_bytes) noexcept {
    return (byte_t*)::operator new(num_bytes, (std::align_val_t)TAlignmentsBytes.biggest_alignment);
  }
  eden_always_inline static constexpr void deallocate_at(byte_t* alloc, sz_t alloc_size_bytes) noexcept { ::operator delete(alloc, alloc_size_bytes, (std::align_val_t)TAlignmentsBytes.biggest_alignment); }

  eden_always_inline constexpr void deallocate() noexcept { deallocate_at(begins[buffer_begin_idx], buffer_size_bytes); }

  eden_always_inline constexpr void
  destroy_all() {
    if(size_each == 0) return;
    detail::destroy_all<Ts...>(begins, size_each, TAlignmentsBytes.map_to_idx);
  }

  void expand_to(sz_t new_capacity_each) {
    assume_assert(new_capacity_each >= capacity_each);
    auto const old_buffer_size_bytes = buffer_size_bytes;
    buffer_size_bytes = new_capacity_each *TSizesBytes.total_size; capacity_each = new_capacity_each;
    auto const new_alloc = alloc_new_buffer(buffer_size_bytes);
    auto const old_buff = begins[buffer_begin_idx];
    detail::move_construct_slices<Ts...>(begins, new_alloc, size_each, TAlignmentsBytes.map_to_idx, capacity_each);
    deallocate_at(old_buff, old_buffer_size_bytes);
  }

  void alloc_from_empty(sz_t new_capacity_each = FirstAllocCapacity) {
    assume_assert(begins[0] == nullptr); assume_assert(size_each == 0); assume_assert(capacity_each == 0); assume_assert(buffer_size_bytes == 0);
    buffer_size_bytes = new_capacity_each * TSizesBytes.total_size;
    capacity_each = new_capacity_each;
    begins[buffer_begin_idx] = alloc_new_buffer(buffer_size_bytes);
    for(auto i{1uz}; i<NumTs; ++i)
      begins[map_idx(i)] = begins[map_idx(i-1)] + new_capacity_each * TSizesBytes[map_idx(i-1)];
  }

public:

  constexpr contiguous_soa() = default;

  template <sz_t N>
  explicit contiguous_soa(flags::ReserveInitial<N>)
  { alloc_from_empty(N); }

  constexpr explicit
  contiguous_soa(contiguous_soa&& other) noexcept
  : size_each(other.size_each), capacity_each(other.capacity_each), buffer_size_bytes(other.buffer_size_bytes) {
    for (auto i{0uz}; i<NumTs; ++i) {
      begins[i] = other.begins[i];
      other.begins[i] = nullptr;
    }
    other.size_each = 0; other.capacity_each = 0; other.buffer_size_bytes = 0;
  }

  contiguous_soa&
  operator=(contiguous_soa&& other) noexcept  {
    destroy_all(); deallocate();
    size_each = other.size_each; capacity_each = other.capacity_each; buffer_size_bytes = other.buffer_size_bytes;
    for (auto i{0uz}; i<NumTs; ++i) {
      begins[i] = other.begins[i];
      other.begins[i] = nullptr;
    }

    other.size_each = 0; other.capacity_each = 0; other.buffer_size_bytes = 0;
    return *this;
  }

  constexpr ~contiguous_soa() {
    destroy_all();
    deallocate();
  }

  eden_always_inline [[nodiscard]] constexpr sz_t total_size()          const noexcept { return size_each * NumTs; }
  eden_always_inline [[nodiscard]] constexpr sz_t individual_size()     const noexcept { return size_each; }
  eden_always_inline [[nodiscard]] constexpr sz_t total_capacity()      const noexcept { return capacity_each * NumTs; }
  eden_always_inline [[nodiscard]] constexpr sz_t individual_capacity() const noexcept { return capacity_each; }
  eden_always_inline [[nodiscard]] constexpr bool empty()               const noexcept { return size_each == 0; }
  eden_always_inline               constexpr void clear()                     noexcept { destroy_all(); }

  template <class T> eden_always_inline [[nodiscard]] constexpr T&       front()        noexcept { assume_assert(size_each not_eq 0); return launder_castT(begins[mapped_idx<T>])[0]; }
  template <class T> eden_always_inline [[nodiscard]] constexpr T const& front()  const noexcept { assume_assert(size_each not_eq 0); return launder_castT(begins[mapped_idx<T>])[0]; }
  template <class T> eden_always_inline [[nodiscard]] constexpr T&       back()         noexcept { assume_assert(size_each not_eq 0); return launder_castT(begins[mapped_idx<T>])[size_each-1]; }
  template <class T> eden_always_inline [[nodiscard]] constexpr T const& back()   const noexcept { assume_assert(size_each not_eq 0); return launder_castT(begins[mapped_idx<T>])[size_each-1]; }
  template <class T> eden_always_inline [[nodiscard]] constexpr T*       data()         noexcept { return launder_castT(begins[mapped_idx<T>]); }
  template <class T> eden_always_inline [[nodiscard]] constexpr T const* data()   const noexcept { return launder_castT(begins[mapped_idx<T>]); }

  template <class T> eden_always_inline [[nodiscard]] constexpr explicit operator  std::span<T>()             noexcept { return std::span(launder_castT(begins[mapped_idx<T>]), size_each); }
  template <class T> eden_always_inline [[nodiscard]] constexpr explicit operator  std::span<T const>() const noexcept { return std::span(launder_castT(begins[mapped_idx<T>]), size_each); }
  template <class T> eden_always_inline [[nodiscard]] constexpr std::span<T>       to_span()                  noexcept { return operator std::span<T>(); }
  template <class T> eden_always_inline [[nodiscard]] constexpr std::span<T const> to_span()            const noexcept { return operator std::span<const T>(); }

  void reserve(sz_t new_capacity_each) noexcept {
    if (begins[0] == nullptr)
      alloc_from_empty(new_capacity_each);
    else if (capacity_each < new_capacity_each)
      expand_to(new_capacity_each);
  }

  template <class... ArgTuples> // should be tuples of arguments, one tuple per member
  void emplace_back(ArgTuples&&... args_for_each_element) {
    if(size_each == 0) alloc_from_empty();
    else if(size_each == capacity_each) expand_to(capacity_each * ExpansionMult);
    detail::add_to_end<Ts...>(begins, size_each, TAlignmentsBytes.map_to_idx, std::forward<ArgTuples>(args_for_each_element)...);
    ++size_each;
  }

  eden_always_inline void push_back(Ts&&... new_elements) { return emplace_back(std::forward_as_tuple(new_elements)...); }
  eden_always_inline void push_back(Ts const&... new_elements) { return emplace_back(std::forward_as_tuple(new_elements)...); }

  template<class T> eden_always_inline constexpr T&
  get(sz_t idx) noexcept {
    assume_assert(idx < size_each);
    return launder_castT(begins[ mapped_idx<T> ])[idx];
  }

  template<class T>
  constexpr T&
  get_at(sz_t idx) eden_throws(std::out_of_range) {
    if (idx >= size_each)
      throw std::out_of_range(std::format("Element access at index {} in eden::contiguous_soa with individual size of {}.", idx, individual_size()));
    return launder_castT(begins[ mapped_idx<T> ])[idx];
  }

};
}
#undef launder_cast
#undef launder_castT
