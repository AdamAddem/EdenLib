#pragma once
#include "typedefs.hpp"
#include "macros.hpp"
#include <cmath>

namespace eden {

// T must not contain any padding
template <class T>
edenNodiscardCXPR
bool areBitwiseEqual(T const* first, T const* second) {
  auto const p1 =     (byte_t const*)  first;
  auto const p1_end = (byte_t const*) (first + 1);
  auto const p2 =     (byte_t const*)  second;
  auto const p2_end = (byte_t const*) (second + 1);
  return std::equal(p1, p1_end, p2, p2_end);
}

// T must not contain any padding
// first and second may not overlap
template <class T>
edenNodiscardCXPR bool
areBitwiseEqualRestrict(T const* edenRestrict first, T const* edenRestrict second) {
  auto const p1 =     (byte_t const*)  first;
  auto const p1_end = (byte_t const*) (first + 1);
  auto const p2 =     (byte_t const*)  second;
  auto const p2_end = (byte_t const*) (second + 1);
  return std::equal(p1, p1_end, p2, p2_end);
}

template <std::floating_point T>
requires (sizeof(T) > sizeof(float))
edenNodiscardCXPR bool
canBeRepresentedByFloat(T val) {
  auto const as_float = (float) val;
  return std::isfinite( as_float ) and (val == (T) as_float);
}

}

