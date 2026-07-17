#pragma once
#include "../typedefs.hpp"
#include "concepts.hpp"
#include "../macros.hpp"

namespace eden {


namespace detail {

template <class T, class First, class... Rest>
static consteval sz_t idx_in_pack_impl() {
  if constexpr(same_c<T, First>) return 0;
  else return idx_in_pack_impl<T, Rest...>() + 1;
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

template <class... Ts>
struct SizePack {
  static constexpr auto NumTs = sizeof...(Ts);
  sz_t sizes[NumTs];
  sz_t total_size{};
  sz_t biggest_size{};
  consteval SizePack() {
    detail::fill_with_type_sizes<Ts...>(sizes);
    for(auto size : sizes) {
      biggest_size = std::max(biggest_size, size);
      total_size += size;
    }
  }

  eden_always_inline [[nodiscard]] constexpr sz_t
  operator[](sz_t idx) const noexcept
  { return sizes[idx]; }
};

template <class... Ts>
struct AlignPack {
  static constexpr auto NumTs = sizeof...(Ts);
  static constexpr auto biggest_alignment = max_align_in_pack<Ts...>;
  sz_t alignments[NumTs];
  sz_t map_to_idx[NumTs];

  // don't ask me how any of this works I totally forgot
  consteval AlignPack() noexcept {
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
        if( std::bit_width(alignment) == i + 1 ) {
          --alignment_count;
          map_to_idx[j] = --num;
        }
      }
      ++i;
    }
  }

  eden_always_inline [[nodiscard]] constexpr
  sz_t operator[](sz_t idx) const noexcept
  { return alignments[idx]; }
};






}