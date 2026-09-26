#pragma once
#include "../typedefs.hpp"
#include "base_vector.hpp"

namespace eden {

// See base_vector.hpp for settings description
template<bool Small = false, u64_t ExpansionMult = 2>
struct vector_settings { static constexpr base_vector_settings<Small, ExpansionMult> base_settings{}; };

// Mostly standard vector implementation
template <class T, vector_settings settings = vector_settings{}, allocator_for_c<T> Allocator = BasicAllocator<T>>
class vector : public base_vector<T, vector<T, settings, Allocator>, settings.base_settings, Allocator> {
  using base = base_vector<T, vector, settings.base_settings, Allocator>;
public:
  using base::base;

  template <class T_, vector_settings settings_, allocator_for_c<T_> Allocator_> friend class vector;

  // Must not access 'this' anymore, as its lifetime as a vector of T has ended.
  // Instead, access through the returned reference.
  // This is a very dangerous method, do not use unless you know what you are doing.
  // You must ensure that 'this' does not get destroyed, but rather the returned reference does.
  // calls clear() so old T's get destroyed and the transmutated vector is empty.
  template <class X, template <class> class Allocator_>
  requires (sizeof(X) == sizeof(T) and alignof(X) == alignof(T))
  vector<X, settings, Allocator_<X>>& transmutate_into_vector_of(this vector<T, settings, Allocator_<T>>&& v) noexcept {
    using transmutated_type = vector<X, settings, Allocator_<X>>;

    auto const begin = v.m_begin;
    auto const size = v.m_size;
    auto const cap = v.m_cap;
    auto const num_elements = v.size();

    v.clear();
    v.unsafe_zero_members();
    v.~vector();
    auto& new_vec = * new (&v) transmutated_type;

    new_vec.m_begin = std::start_lifetime_as_array<X>(begin, num_elements);
    if constexpr(settings.base_settings.is_small) {
      new_vec.m_size = 0;
      new_vec.m_cap = cap;
    }
    else {
      new_vec.m_size = (X*) begin;
      new_vec.m_cap = (X*) cap;
    }

    return *std::launder( &new_vec );
  }

};

template <class T>
using vector16 = vector<T, vector_settings<true>{}>;

}