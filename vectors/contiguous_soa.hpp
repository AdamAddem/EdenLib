#pragma once
#include "../metaprogramming/packs.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"

#include <cstring>
#include <exception>
#include <format>
#include <utility>
#include <span>
#include <tuple>
#include <stdexcept>
#include <new>
#include <type_traits>
#include <cassert>

namespace eden {

#define launder_cast(Type, ptr) std::launder( (Type*) (ptr) )
#define launder_castT(ptr) std::launder( (T*) (ptr) )

namespace detail {

template <class First, class... Rest>
eden_always_inline static constexpr void
destroy_all(byte_t** begins, sz_t size, sz_t const* idx_arr) noexcept {
  if(size == 0) return;
  if constexpr(sizeof...(Rest)) destroy_all<Rest...>(begins, size, idx_arr + 1);

  if constexpr(not std::is_trivially_destructible_v<First>) {
    First* const first_begin = (First*) begins[ idx_arr[0] ];
    First* end = (First*) first_begin + size;
    while(true) {
      --end;
      std::destroy_at( launder_cast(First, end) );
      if(end == first_begin) break;
    }
  }
}

template <class First, class... Rest>
eden_always_inline static constexpr void
relocate_slices(byte_t** old_slices, byte_t* new_slice, sz_t size_each, sz_t const* idx_arr, sz_t new_capacity_each) noexcept {
  auto const my_idx = idx_arr[0];
  
  if(size_each not_eq 0) {
    First* old_slice = launder_cast(First, old_slices[my_idx]);
    First* construct_location = (First*) new_slice;
  
    if constexpr(eden_trivially_relocatable(First))
      std::memcpy( construct_location, old_slice, size_each * sizeof(First) );
    else
      for(auto i{0uz}; i<size_each; ++i)
        std::construct_at( construct_location + i, std::move(old_slice[i]) ), 
        std::destroy_at(old_slice + i); // technically not reverse order but who really cares
  }
  old_slices[my_idx] = new_slice;

  if constexpr(sizeof...(Rest)) {
    new_slice += new_capacity_each * sizeof(First);
    relocate_slices<Rest...>(old_slices, new_slice, size_each, idx_arr + 1, new_capacity_each);
  }
}

template <class First, class... Rest>
eden_always_inline static constexpr void
add_to_end(byte_t** begins, sz_t size_each, sz_t const* idx_arr, auto&& first_args, auto&&... rest_args) noexcept {
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

// TODO: 
// - Add settings and custom allocator support
// - Fix undefined behavior occuring when calling data() or span() on empty vector due to launder_cast
// - 
// 
template <class... Ts>
requires (sizeof...(Ts) > 1)
class contiguous_soa {
  static constexpr auto NumTs = sizeof...(Ts);
  static constexpr auto ExpansionMult = 2;
  static constexpr packs::SizePack<Ts...> Sizes{};
  static constexpr packs::AlignPack<Ts...> Alignments{};
  static constexpr auto FirstAllocCapacity = 1;
  static constexpr auto buffer_begin_idx = Alignments.map_to_idx[0];

  eden_always_inline [[nodiscard]] static constexpr sz_t map_idx(sz_t raw_idx) { return Alignments.map_to_idx[raw_idx]; }

  /*
  template <class T>
  requires is_one_of<T, Ts...>
  static constexpr sz_t mapped_idx = map_idx( idx_in_pack<T, Ts...> ); */

  template <sz_t IDX>
  using type_at_idx = packs::type_at_idx<IDX, Ts...>;

  byte_t* begins[NumTs]{};
  sz_t    size_each{};
  sz_t    capacity_each{};
  sz_t    buffer_size_bytes{};

  eden_always_inline [[nodiscard]] static byte_t* alloc_new_buffer(sz_t num_bytes) noexcept { return (byte_t*)::operator new(num_bytes, (std::align_val_t)Alignments.biggest_alignment); }
  eden_always_inline static constexpr void deallocate_at(byte_t* alloc, sz_t alloc_size_bytes) noexcept { ::operator delete(alloc, alloc_size_bytes, (std::align_val_t)Alignments.biggest_alignment); }
  eden_always_inline constexpr void deallocate() noexcept { deallocate_at(begins[buffer_begin_idx], buffer_size_bytes); }


  eden_always_inline static constexpr void
  destroy_all_at(byte_t** begins, sz_t size_each) noexcept {
    detail::destroy_all<Ts...>(begins, size_each, Alignments.map_to_idx);
  }

  eden_always_inline constexpr void destroy_all() noexcept { destroy_all_at(begins, size_each); size_each = 0; }

  void expand_to(sz_t new_capacity_each) noexcept {
    assert(new_capacity_each >= capacity_each);
    auto const old_buffer_size_bytes = buffer_size_bytes;
    capacity_each = new_capacity_each;
    buffer_size_bytes = new_capacity_each * Sizes.total_size; 
    assert(buffer_size_bytes >= old_buffer_size_bytes);

    auto const new_alloc = alloc_new_buffer(buffer_size_bytes);
    auto const old_buff = begins[buffer_begin_idx];

    detail::relocate_slices<Ts...>(begins, new_alloc, size_each, Alignments.map_to_idx, capacity_each);
    deallocate_at(old_buff, old_buffer_size_bytes);
  }

  void alloc_from_empty(sz_t new_capacity_each = FirstAllocCapacity) noexcept {
    assert(begins[0] == nullptr); assert(size_each == 0); assert(capacity_each == 0); assert(buffer_size_bytes == 0);
    buffer_size_bytes = new_capacity_each * Sizes.total_size;
    capacity_each = new_capacity_each;
    begins[buffer_begin_idx] = alloc_new_buffer(buffer_size_bytes);
    for(auto i{1uz}; i<NumTs; ++i)
      begins[map_idx(i)] = begins[map_idx(i-1)] + new_capacity_each * Sizes[map_idx(i-1)];
  }

public:

  constexpr contiguous_soa() = default;

  template <sz_t N>
  explicit contiguous_soa(flags::ReserveInitial<N>) noexcept
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

  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto&       front()        noexcept { assert(size_each not_eq 0); return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] )[0]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto const& front()  const noexcept { assert(size_each not_eq 0); return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] )[0]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto&       back()         noexcept { assert(size_each not_eq 0); return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] )[size_each-1]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto const& back()   const noexcept { assert(size_each not_eq 0); return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] )[size_each-1]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto*       data()         noexcept { return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] ); }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto const* data()   const noexcept { return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] ); }

  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr std::span< type_at_idx<IDX> >      to_span()                 noexcept { return std::span( launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] ), size_each); }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr std::span< type_at_idx<IDX> const> to_span()           const noexcept { return std::span( launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] ), size_each); }


  void reserve(sz_t new_capacity_each) noexcept {
    if(capacity_each >= new_capacity_each) return;
    if (begins[0] not_eq nullptr)
      expand_to(new_capacity_each);
    else
      alloc_from_empty(new_capacity_each);     
  }

  template <class... ArgTuples> // should be tuples of arguments, one tuple per member
  requires (sizeof...(ArgTuples) == NumTs)
  void emplace_back(ArgTuples&&... args_for_each_element) noexcept {
    if(capacity_each == 0) alloc_from_empty();
    else if(size_each == capacity_each) expand_to(capacity_each * ExpansionMult);
    detail::add_to_end<Ts...>(begins, size_each, Alignments.map_to_idx, std::forward<ArgTuples>(args_for_each_element)...);
    ++size_each;
  }

  template <class... Args>
  eden_always_inline void push_back(Args&&... new_elements) noexcept { 
    return emplace_back(
      std::forward_as_tuple(std::forward<Args>(new_elements))...
    ); 
  }

  template<sz_t IDX> eden_always_inline constexpr auto&
  get(sz_t idx) noexcept {
    assert(idx < size_each);
    return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] )[idx];
  }

  template<sz_t IDX> constexpr auto&
  get_at(sz_t idx) eden_throws(std::out_of_range) {
    if (idx >= size_each)
      throw std::out_of_range( std::format("Element access at index {} in eden::contiguous_soa with individual size of {}.", idx, individual_size()) );
    return launder_cast( type_at_idx<IDX>, begins[map_idx(IDX)] )[idx];
  }

};
}
#undef launder_cast
#undef launder_castT
