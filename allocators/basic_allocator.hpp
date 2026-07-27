#pragma once
#include "../macros.hpp"
#include "../metaprogramming/concepts.hpp"
#include "../typedefs.hpp"

#include <memory>
#include <new>
#include <vector>

namespace eden {

// specialize this class for your type T to describe how you'd like a container to handle your type
template <class T>
struct ContainerPreferences {
  static constexpr bool can_elide_destruction =
    requires { requires T::can_elide_destruction; } or std::is_trivially_destructible_v<T>;

  static constexpr bool never_relocate = requires { requires T::never_relocate; };
};

template <class T>
concept allocator_c = requires {
  std::declval<T>().allocate(1, align_t{2}); // must be able to allocate with alignment specified

  // must be able to deallocate with combinations of size and alignment
  std::declval<T>().deallocate((void*)0, sz_t{0});
  std::declval<T>().deallocate((void*)0, align_t{2});
  std::declval<T>().deallocate((void*)0, sz_t{0}, align_t{2});

  typename T::value_type;
  T::exclusive_use;           // if true, any container using the allocator has exclusive access to its resources
  T::may_reasonably_fail;     // if true, the allocator may fail for reasons other than rare external factors such as being OOM
  T::requires_deallocation;   // if false, avoiding deallocation is safe and/or doesn't cause memory leaks (ex, arena)
  T::stateless;

  // if true, has 'reallocate' function taking pointer to old buffer and the parameters for a new allocation.
  // if the old buffer satisfies the requirements for the new allocation, just returns pointer to the old buffer.
  // otherwise, returns a pointer to a newly allocated buffer (but does not deallocate the old buffer).
  T::supports_reallocate;
  T::supports_allocate_raw;   // if true, has 'allocate_raw' function returning byte_t*
};

template <class T>
struct BasicAllocator {
  using value_type = T;
  static constexpr bool exclusive_use = false;
  static constexpr bool may_reasonably_fail = false;
  static constexpr bool requires_deallocation = true;
  static constexpr bool stateless = true;
  static constexpr bool supports_allocate_raw = true;
  static constexpr bool supports_reallocate = false;

  static constexpr auto TAlign = align_t{alignof(T)};

  eden_always_inline [[nodiscard]] static T*
  allocate(sz_t count, align_t alignment = TAlign) noexcept
  { return std::start_lifetime_as_array<T>( (T*) ::operator new[](count, alignment), count ); }

  eden_always_inline [[nodiscard]] static byte_t*
  allocate_raw(sz_t byte_count, align_t alignment) noexcept
  { return std::start_lifetime_as_array<byte_t>( (byte_t*) ::operator new[](byte_count, alignment), count); }

  eden_always_inline static void
  deallocate(void* allocated, align_t allocated_alignment = TAlign) noexcept
  { ::operator delete[](allocated, allocated_alignment); }

  eden_always_inline static void
  deallocate(void* allocated, sz_t allocated_count) noexcept
  { ::operator delete[](allocated, allocated_count); }

  eden_always_inline static void
  deallocate(void* allocated, sz_t allocated_count, align_t allocated_alignment) noexcept
  { ::operator delete[](allocated, allocated_count, allocated_alignment); }

}; static_assert(allocator_c<BasicAllocator<int>>);

}