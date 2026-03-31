#pragma once
#include "concepts.hpp"
#include "type_flags.hpp"
#include <utility>

namespace eden {

template <ExclusivityFlag lower = Inclusive, ExclusivityFlag higher = Inclusive, enum_c T>
[[nodiscard]] constexpr bool
enumBetween(T value, T lower_bound, T higher_bound) {
  return [&]() constexpr {
    if constexpr (std::is_same_v<lower, Exclusive>)
      return std::to_underlying(value) > std::to_underlying(lower_bound);
    else
      return std::to_underlying(value) >= std::to_underlying(lower_bound);
  }()
  and
  [&]() constexpr {
    if constexpr (std::is_same_v<higher, Exclusive>)
      return std::to_underlying(value) < std::to_underlying(higher_bound);
    else
      return std::to_underlying(value) <= std::to_underlying(higher_bound);
  }();
}

}