#pragma once
#include "typedefs.hpp"

template<sz_t N>
requires (N > 0)
struct reserve_initial_def{};

template<sz_t N>
static constexpr reserve_initial_def<N> reserve_initial;