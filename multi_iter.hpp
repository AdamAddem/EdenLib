#pragma once
#include "edenlib/metaprogramming/concepts.hpp"
#include "macros.hpp"
#include "typedefs.hpp"

#include <cassert>
#include <span>
#include <tuple>
#include <utility>

namespace eden {

namespace detail {

template<class... Ts, sz_t... I>
edenAlwaysInline auto makeReferencesImpl(std::tuple<Ts...> const& t, std::index_sequence<I...>) { return std::tie(*std::get<I>(t)...); }

template<class... Ts>
edenAlwaysInline auto makeReferences(std::tuple<Ts...> const& t) {
  return makeReferencesImpl(t, std::make_index_sequence<sizeof...(Ts)>{});
}

template<class... Ts, sz_t... I>
edenAlwaysInline void plusplusAllImpl(std::tuple<Ts...>& t, std::index_sequence<I...>) { (..., ++std::get<I>(t)); }

template<class... Ts>
edenAlwaysInline void plusplusAll(std::tuple<Ts...>& t) {
  return plusplusAllImpl(t, std::make_index_sequence<sizeof...(Ts)>{});
}

edenAlwaysInline bool allEqual(sz_t first, same_c<sz_t> auto... rest) {
  return ( (first == rest) && ... );
}

template<class... Ts>
class all_iter {
  using TupleOfPtrs = std::tuple<Ts*...>;
  using TupleOfRefs = std::tuple<Ts&...>;
  TupleOfPtrs ptrs;
public:

  constexpr all_iter(TupleOfPtrs ptrs) noexcept : ptrs(ptrs) {}

  [[nodiscard]] constexpr TupleOfRefs
  operator*() const noexcept {
    return makeReferences(ptrs);
  }

  constexpr void operator++() noexcept { plusplusAll(ptrs); }
  constexpr bool operator==(all_iter const&) const noexcept = default;
};

template <class... Ts, sz_t... I>
edenAlwaysInline all_iter<Ts...> begin_impl(std::tuple<std::span<Ts>...>& spans, std::index_sequence<I...>) {
  return { std::tuple<Ts*...>( std::get<I>(spans).data()... ) };
}

template <class... Ts, sz_t... I>
edenAlwaysInline all_iter<Ts...> end_impl(std::tuple<std::span<Ts>...>& spans, std::index_sequence<I...>) {
  return { std::tuple<Ts*...>( std::get<I>(spans).end().base()... ) };
}

template<class... Ts>
struct multi_iter_maker {
  using idx_seq = std::make_index_sequence<sizeof...(Ts)>;

  std::tuple<std::span<Ts>...> spans;

  edenAlwaysInline constexpr explicit multi_iter_maker(std::span<Ts>... spans) noexcept : spans(spans...) {}

  edenInlineNodiscardCXPR all_iter<Ts...> begin() noexcept { return begin_impl(spans, idx_seq{}); }
  edenInlineNodiscardCXPR all_iter<Ts...> end()   noexcept { return end_impl(spans, idx_seq{}); }

  // edenInlineNodiscardCXPR pair_iter<A const, B const> begin() const noexcept { return {as.data(), bs.data()}; }
  // edenInlineNodiscardCXPR pair_iter<A const, B const> end()   const noexcept { return {as.end().base(), bs.end().base()}; }
};
}

// Use as such:
//    std::span<int> ints = ...;
//    std::span<float> floats = ...;
//    for(auto [i, f] = iter_over(ints, floats) ) { ... }
// i and f will be int& and float& respectively, do not do 'auto& [i, f]'
// Each span must have the same size.
#define pre assert(detail::allEqual(spans.size()...));
template<class... Ts>
edenAlwaysInline constexpr auto iter_over(std::span<Ts>... spans) noexcept { pre
  return detail::multi_iter_maker{spans...};
}
#undef pre

}