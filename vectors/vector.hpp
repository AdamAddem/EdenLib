#pragma once
#include "../typedefs.hpp"
#include "base_vector.hpp"

namespace eden {

// See base_vector.hpp for settings description
template<bool Small = false, u64_t ExpansionMult = 2>
struct vector_settings { static constexpr base_vector_settings<Small, ExpansionMult> base_settings{}; };

// Mostly standard vector implementation
template <class T, auto settings = vector_settings{}, allocator_for_c<T> Allocator = std::allocator<T>>
class vector : public base_vector<T, vector<T, settings, Allocator>, settings.base_settings, Allocator> {};

template <class T>
using vector16 = vector<T, vector_settings<true>{}>;

}