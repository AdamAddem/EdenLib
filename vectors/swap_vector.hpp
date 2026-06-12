#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../metaprogramming/type_class.hpp"
#include "../typedefs.hpp"
#include "base_vector.hpp"

#include <memory>
#include <print>
#include <string>

namespace eden {

/*
*  Stability (default 4, recommended to be a power of 2 for faster integer division):
*     -When a searched for element is found, it is swapped with the element size()/stability + 1 indices closer to the back.
*     -A stability of 1 always swaps the element with the backmost element.
*     -A higher stability is recommended for patterns that consistently search for a larger subset of elements over a longer period of time.
*     -A lower stability is recommended for patterns that search for a smaller subset of elements that changes frequently.
*  Small, ExpansionMult: (refer to base_vector.hpp)
*/
template<u64_t Stability = 4,
         bool Small = false,
         u64_t ExpansionMult = 2>
requires (Stability >= 1)
struct swap_vector_settings {
  static constexpr u64_t stability = Stability;
  static constexpr base_vector_settings<Small, ExpansionMult> base_settings{};
};

template <class T, auto BinaryPredicate, auto settings = swap_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires std::swappable<T>
class swap_vector : public base_vector<T, swap_vector<T, BinaryPredicate, settings, Allocator>, settings.base_settings, Allocator> {
  static constexpr auto stability = settings.stability;
  using base = base_vector<T, swap_vector, settings.base_settings, Allocator>;
  friend class base_vector<T, swap_vector, settings.base_settings, Allocator>;
  using base::m_begin;

public:
  using base::base;
  using typename base::count_t;

  // returns nullptr if not found. pointer is unstable and may point to bad data should this vector expand or another search be called.
  template<class ...KeyTypes>
  [[nodiscard]] constexpr T*
  search(KeyTypes&&... key)
  noexcept(nothrow_swappable_c<T>)
  requires (convertible_to_c<std::invoke_result_t<decltype(BinaryPredicate), T const&, KeyTypes...>, bool>) {
    static constexpr bool transposition = stability >= 1;

    auto const sz = this->size();
    auto n = sz;
    auto const back_bubble = transposition ? sz / stability + 1 : 0;
    while (n-- > 0) {
      auto curr_ptr = this->m_begin + n;
      bool const hit = BinaryPredicate(*curr_ptr, std::forward<KeyTypes>(key)...);
      if (hit) {
        auto const swap_location = transposition ? this->m_begin + std::min(back_bubble + n, sz-1) : &this->back();
        if (curr_ptr not_eq swap_location) std::swap(*curr_ptr, *swap_location);
        return swap_location;
      }
    }
    return nullptr;
  }

  // returns nullptr if not found. pointer is unstable and may point to bad data should this vector expand or another search be called.
  template<class ...KeyTypes>
  [[nodiscard]] constexpr T*
  search(auto&& OtherPredicate, KeyTypes&&... key)
  noexcept(nothrow_swappable_c<T>)
  requires (convertible_to_c<std::invoke_result_t<decltype(OtherPredicate), T const&, KeyTypes...>, bool>) {
    static constexpr bool transposition = stability >= 1;

    auto const sz = this->size();
    auto n = sz;
    auto const back_bubble = transposition ? sz / stability + 1 : 0;
    while (n-- > 0) {
      auto curr_ptr = this->m_begin + n;
      bool const hit = OtherPredicate(*curr_ptr, std::forward<KeyTypes>(key)...);
      if (hit) {
        auto const swap_location = transposition ? this->m_begin + std::min(back_bubble + n, sz-1) : &this->back();
        if (curr_ptr not_eq swap_location) std::swap(*curr_ptr, *swap_location);
        return swap_location;
      }
    }
    return nullptr;
  }

  // will swap each element into the index specified by GetIndexOf
  // only really useful if the type holds some sort of unique id
  constexpr void
  sort_by_idx(auto&& GetIndexOf)
  noexcept(nothrow_swappable_c<T>)
  requires (convertible_to_c<std::invoke_result_t<decltype(GetIndexOf), T const&>, count_t>) {
    count_t curr_idx{};
    auto const sz = this->size();
    while (curr_idx < sz) {
      auto& curr = m_begin[curr_idx];
      count_t const idx = GetIndexOf(curr); assume_assert(idx < sz);
      if (idx not_eq curr_idx)
        std::swap(curr, m_begin[idx]);

      ++curr_idx;
    }
  }

};

}
