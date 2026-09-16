#pragma once
#include "../typedefs.hpp"
#include "concepts.hpp"
#include "../macros.hpp"

namespace eden::packs {


namespace detail {

template <class T, class First, class... Rest>
static consteval sz_t idx_in_pack_impl() {
  if constexpr(same_c<T, First>) return 0;
  else return idx_in_pack_impl<T, Rest...>() + 1;
}

template <class T>
struct Fake { using type = T; };

template<sz_t IDX, class First, class... Rest>
static consteval auto type_at_idx_impl() {
  if constexpr(IDX == 0) return Fake<First>{};
  else return type_at_idx_impl<IDX - 1, Rest...>();
}

template <class... Ts> struct max_align_in_pack_impl{ alignas(Ts...) char x; };

template <class First, class... Rest>
static consteval void fill_with_type_sizes(sz_t* size_arr) {
  size_arr[0] = sizeof(First);
  if constexpr(sizeof...(Rest)) fill_with_type_sizes<Rest...>(size_arr + 1);
}

template <class First, class... Rest>
static consteval void fill_with_type_alignments(sz_t* alignment_arr) {
  alignment_arr[0] = alignof(First);
  if constexpr(sizeof...(Rest)) fill_with_type_alignments<Rest...>(alignment_arr + 1);
}

}

template <class T, class... Ts> static constexpr sz_t idx_in_pack = detail::idx_in_pack_impl<T, Ts...>();
template <class... Ts> static constexpr sz_t max_align_in_pack = alignof(detail::max_align_in_pack_impl<Ts...>);

template <sz_t IDX, class... Ts>
using type_at_idx = decltype(detail::type_at_idx_impl<IDX, Ts...>())::type;

template <class... Ts>
struct PackData {
  static constexpr auto NumTs = sizeof...(Ts);
  static constexpr auto biggest_alignment = max_align_in_pack<Ts...>;
  sz_t sizes[NumTs];
  sz_t total_size{};
  sz_t biggest_size{};
  
  sz_t alignments[NumTs];
  sz_t typeidx_to_alignidx[NumTs]; // maps a T's index in Ts to its index in ATs, where ATs is the parameter pack Ts sorted from biggest to smallest alignment
  sz_t alignidx_to_typeidx[NumTs]; // maps an index in AT's back to an index in Ts
  sz_t alignidx_to_offset[NumTs];  // maps an index in AT's to its byte offset in a std::tuple<ATs...>

  // Ts:      int, char, void*
  // aligned: void*, int, char
  // t_to_a:  [1, 2, 0]
  // a_to_t:  [2, 0, 1]
  // a_to_o: [0, 8, 12]

  // don't ask me how any of this works I totally forgot
  consteval PackData() noexcept {

    // initialize sizes
    {
      detail::fill_with_type_sizes<Ts...>(sizes);
      for(auto size : sizes) {
        biggest_size = std::max(biggest_size, size);
        total_size += size;
      }
    }
    
    detail::fill_with_type_alignments<Ts...>(alignments);

    static constexpr auto alignments_count_sz = std::bit_width(biggest_alignment);
    sz_t alignments_count[ alignments_count_sz ]{};
    for(auto i{0uz}; i<NumTs; ++i)
      alignments_count[ std::bit_width( alignments[i] ) - 1 ] += 1;

    auto i{0uz};
    auto j{0uz};
    auto num{NumTs};
    while (i < alignments_count_sz) {
      auto& alignment_count = alignments_count[i];
      for(j = 0; j<NumTs and alignment_count not_eq 0; ++j) {
        auto const alignment = alignments[j];
        if( (sz_t) std::bit_width(alignment) == i + 1 ) {
          --alignment_count;
          typeidx_to_alignidx[j] = --num;
        }
      }

      ++i;
    }

    for(i = 0; i<NumTs; ++i)
      alignidx_to_typeidx[typeidx_to_alignidx[i]] = i;

    alignidx_to_offset[0] = 0;
    for(i = 1; i<NumTs; ++i) alignidx_to_offset[i] = alignidx_to_offset[i-1] + sizes[ alignidx_to_typeidx[i-1] ];
  }

};



}