#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../metaprogramming/type_class.hpp"
#include "../typedefs.hpp"
#include "base_vector.hpp"

#include <memory>
#include <string>

namespace eden {

/*
*  Stability (default 4, recommended to be a power of 2 for faster integer division):
*     -When a searched for element is found, it is swapped with the element size()/stability + 1 indices closer to the back.
*     -A stability of 1 always swaps the element with the backmost element.
*     -A higher stability is recommended for consistent searches of a larger subset of elements over a longer period of time.
*     -A lower stability is recommended for volatile searches of a smaller subset of elements.
*  PreserveBackmost (default false):
*     -When true, will prevent any found elements from swapping into the backmost position. Useful if you wish to preserve the most recent addition.
*     -Note that some methods do not respect this when it would not make sense to.
*  Small, ExpansionMult: (refer to base_vector.hpp)
*/
template<u64_t Stability = 4,
         bool PreserveBackmost = false,
         bool Small = false,
         u64_t ExpansionMult = 2>
requires (Stability >= 1)
struct swap_vector_settings {
  static constexpr u64_t stability = Stability;
  static constexpr u64_t preserve_backmost = PreserveBackmost;
  static constexpr base_vector_settings<Small, ExpansionMult> base_settings{};
};

template <class T, auto settings = swap_vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
requires std::swappable<T>
class swap_vector : public base_vector<T, swap_vector<T, settings, Allocator>, settings.base_settings, Allocator> {
  static constexpr auto stability = settings.stability;
  static constexpr auto preserve_back = settings.preserve_backmost;
  using base = base_vector<T, swap_vector, settings.base_settings, Allocator>;
  friend class base_vector<T, swap_vector, settings.base_settings, Allocator>;
  using base::m_begin;

public:
  using base::base;
  using typename base::count_t;

  // returns nullptr if not found. element will be moved backwards.
  template<class ...KeyTypes>
  [[nodiscard]] constexpr T*
  search(auto&& Predicate, KeyTypes&&... keys)
  noexcept(nothrow_swappable_c<T>)
  requires(convertible_to_c<std::invoke_result_t<decltype(Predicate), T const&, KeyTypes...>, bool>) {
    if (this->empty()) return nullptr;

    auto const back_ptr = &this->back();
    if ( Predicate(*back_ptr, keys... ) ) return back_ptr;

    static constexpr bool transposition = stability >= 1;

    auto n = this->size();
    auto const sz = n - static_cast<count_t>(preserve_back);
    --n;

    auto const back_bubble = transposition ? sz / stability + 1 : 0;
    while (n-- > 0) {
      auto curr_ptr = this->m_begin + n;
      bool const hit = Predicate(*curr_ptr, keys...);
      if (hit) {
        auto const swap_location = transposition ? this->m_begin + std::min(back_bubble + n, sz-1) : back_ptr;
        std::swap(*curr_ptr, *swap_location);
        return swap_location;
      }
    }
    return nullptr;
  }

  // returns nullptr if not found. element will be swapped directly into back.
  // does NOT respect PreserveBackmost.
  template<class ...KeyTypes>
  [[nodiscard]] constexpr T*
  search_swapback(auto&& Predicate, KeyTypes&&... keys) noexcept
  requires(convertible_to_c<std::invoke_result_t<decltype(Predicate), T const&, KeyTypes...>, bool>) {
    if (this->empty()) return nullptr;
    auto back_ptr = &this->back();
    if ( Predicate(*back_ptr, keys... ) ) return back_ptr;

    auto n = this->size() - 1;
    while (n-- > 0) {
      auto curr = m_begin + n;
      bool const hit = Predicate(*curr, keys...); // not forwarding keys to prevent moves
      if (hit) {
        std::swap(*curr, *back_ptr);
        return back_ptr;
      }
    }
    return nullptr;
  }

  // returns nullptr if not found. pointer is unstable and may point to bad data should this vector expand or another search be called.
  template<class ...KeyTypes>
  [[nodiscard]] constexpr T*
  search_noswap(auto&& Predicate, KeyTypes&&... key) const noexcept
  requires(convertible_to_c<std::invoke_result_t<decltype(Predicate), T const&, KeyTypes...>, bool>) {
    auto n = this->size();
    while (n-- > 0) {
      auto curr = m_begin + n;
      bool const hit = Predicate(*curr, key...);
      if (hit) return curr;
    }
    return nullptr;
  }

  // will swap each element into the index specified by GetIdxOf.
  // only really useful if the object itself keeps track of its original insertion order.
  // does NOT respect PreserveBackmost.
  constexpr void
  sort_by_unique_idx(auto&& GetIdxOf)
  noexcept(nothrow_swappable_c<T>)
  requires(convertible_to_c<std::invoke_result_t<decltype(GetIdxOf), T const&>, count_t>) {
    count_t curr_idx{};
    auto const sz = this->size();
    while (curr_idx < sz) {
      auto& curr = m_begin[curr_idx];
      count_t const idx = GetIdxOf(curr); assume_assert(idx < sz);
      if (idx not_eq curr_idx)
        std::swap(curr, m_begin[idx]);

      ++curr_idx;
    }
  }


};

}
