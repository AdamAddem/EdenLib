#pragma once
#include "metaprogramming/concepts.hpp"
#include "type_flags.hpp"
#include "macros.hpp"

#include <utility>

namespace eden {

template <ExclusivityFlag lower = flags::Inclusive, ExclusivityFlag higher = flags::Inclusive, enum_c T>
edenAlwaysInline [[nodiscard]] constexpr bool
enumBetween(T value, T lower_bound, T higher_bound) noexcept {
  static constexpr bool lower_exclusive = std::is_same_v<lower, flags::Exclusive>;
  static constexpr bool higher_exclusive = std::is_same_v<higher, flags::Exclusive>;

  bool res;
  if constexpr(lower_exclusive)
    res = std::to_underlying(value) > std::to_underlying(lower_bound);
  else
    res = std::to_underlying(value) >= std::to_underlying(lower_bound);

  if constexpr (higher_exclusive)
    res = res & (std::to_underlying(value) < std::to_underlying(higher_bound));
  else
    res = res & (std::to_underlying(value) <= std::to_underlying(higher_bound));

  return res;
}

template <enum_c T>
edenAlwaysInline [[nodiscard]] consteval bool
enumLessThan(T lower, T higher) {
  return std::to_underlying(lower) < std::to_underlying(higher);
}


namespace detail {
  template <auto first, auto second, auto... values>
  consteval bool enumIncreasingByOneImpl() {
    if constexpr (sizeof...(values) == 0) return second - first == 1;
    if (second - first == 1) return enumIncreasingByOneImpl<second, values...>();
    return false;
  }
}

template <auto... values>
requires ( enum_c<decltype(values)> and ... ) and (sizeof...(values) >= 2)
[[nodiscard]] consteval bool
enumIncreasingByOne() { return detail::enumIncreasingByOneImpl<values...>(); }

}