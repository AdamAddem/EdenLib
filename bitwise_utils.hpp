#pragma once
#include "typedefs.hpp"
#include "macros.hpp"


namespace eden {

// T must not contain any padding
template <class T>
[[nodiscard]] constexpr
bool are_bitwise_equal(T const* first, T const* second) {
  auto const p1 =     (byte_t const*)  first;
  auto const p1_end = (byte_t const*) (first + 1);
  auto const p2 =     (byte_t const*)  second;
  auto const p2_end = (byte_t const*) (second + 1);
  return std::equal(p1, p1_end, p2, p2_end);
}

// T must not contain any padding
// first and second may not overlap
template <class T>
[[nodiscard]] constexpr
bool are_bitwise_equal_restrict(T const* eden_restrict first, T const* eden_restrict second) {
  auto const p1 =     (byte_t const*)  first;
  auto const p1_end = (byte_t const*) (first + 1);
  auto const p2 =     (byte_t const*)  second;
  auto const p2_end = (byte_t const*) (second + 1);
  return std::equal(p1, p1_end, p2, p2_end);
}

}

