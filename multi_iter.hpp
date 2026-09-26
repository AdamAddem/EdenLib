#pragma once
#include "typedefs.hpp"
#include <utility>
#include <span>
#include <cassert>
#include "macros.hpp"

namespace eden {

namespace detail {
template<class A, class B>
class pair_iter {
  A* a;
  B* b;
public:

  constexpr pair_iter(A* a, B* b) noexcept : a(a), b(b) {}

  [[nodiscard]] constexpr std::pair<A&, B&>
  operator*() const noexcept
  { return {*a, *b}; }

  constexpr void operator++() noexcept { ++a; ++b; }
  constexpr bool operator==(pair_iter const&) const noexcept = default;
};

template<class A, class B>
struct pair_iter_maker {
  std::span<A> as;
  std::span<B> bs;
  constexpr pair_iter_maker(std::span<A> as, std::span<B> bs) noexcept : as(as), bs(bs) {}

  edenInlineNodiscardCXPR pair_iter<A, B> begin() noexcept { return {as.data(), bs.data()}; }
  edenInlineNodiscardCXPR pair_iter<A, B> end()   noexcept { return {as.end().base(), bs.end().base()}; }

  // edenInlineNodiscardCXPR pair_iter<A const, B const> begin() const noexcept { return {as.data(), bs.data()}; }
  // edenInlineNodiscardCXPR pair_iter<A const, B const> end()   const noexcept { return {as.end().base(), bs.end().base()}; }
};
}


// Use as such:
//    std::span<int> ints = ...;
//    std::span<float> floats = ...;
//    for(auto [i, f] = iter_over_both(ints, floats) ) { ... }
// i and f will be int& and float& respectively, do not do 'auto& [i, f]'
// Each span must have the same size.
#define pre assert(firsts.size() == seconds.size());
  template<class A, class B>
  constexpr auto iter_over_both(std::span<A> firsts, std::span<B> seconds) noexcept { pre return detail::pair_iter_maker{firsts, seconds}; }
#undef pre

}