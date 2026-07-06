#pragma once
#include "metaprogramming/concepts.hpp"
#include "type_flags.hpp"
#include "macros.hpp"

#include <utility>

namespace eden {

template <ExclusivityFlag lower = flags::Inclusive, ExclusivityFlag higher = flags::Inclusive, enum_c T>
eden_always_inline [[nodiscard]] constexpr bool
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
eden_always_inline [[nodiscard]] consteval bool
enumLessThan(T lower, T higher) {
  return std::to_underlying(lower) < std::to_underlying(higher);
}


}