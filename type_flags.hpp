#pragma once
#include "typedefs.hpp"
#include <type_traits>

namespace eden {

struct Exclusive {Exclusive() = delete;};
struct Inclusive {Inclusive() = delete;};

template <class T>
concept ExclusivityFlag = std::is_same_v<T, Exclusive> or std::is_same_v<T, Inclusive>;

template<sz_t N>
requires (N > 0)
struct reserve_initial_def{};

template<sz_t N>
static constexpr reserve_initial_def<N> reserve_initial;

}