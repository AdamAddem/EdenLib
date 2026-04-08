#pragma once
#include "concepts.hpp"

namespace eden {

namespace flags {
struct Exclusive : type<Exclusive, "Exclusive"> {Exclusive() = delete;};
struct Inclusive : type<Inclusive, "Inclusive"> {Inclusive() = delete;};
struct Less : type<Less, "Less"> {Less() = delete;};
struct LessThan : type<LessThan, "LessThan"> {LessThan() = delete;};
struct Greater : type<Greater, "Greater"> {Greater() = delete;};
struct GreaterThan : type<GreaterThan, "GreaterThan"> {GreaterThan() = delete;};

template <sz_t N>
requires (N > 0)
struct ReserveInitial {
  consteval ReserveInitial() noexcept = default;
};

template <sz_t N>
static constexpr ReserveInitial<N> reserve_initial;
}

template <class T>
concept ExclusivityFlag = type<T>::template is_one_of<flags::Exclusive, flags::Inclusive>;

template <class T>
concept OrderingFlag = type<T>::template is_one_of<flags::Less, flags::LessThan, flags::Greater, flags::GreaterThan>;

}