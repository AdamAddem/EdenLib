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
};

template <class Allocator, class T>
concept allocator_for_c = requires {
  { std::declval<Allocator>().allocate(1, align_t{2}) } -> same_c<T*>;

  // must be able to deallocate with combinations of size and alignment
  std::declval<Allocator>().deallocate( (T*)0, sz_t{0});
  std::declval<Allocator>().deallocate( (T*)0, align_t{2});
  std::declval<Allocator>().deallocate( (T*)0, sz_t{0}, align_t{2});

  typename Allocator::value_type;
  Allocator::exclusive_use;           // if true, any container using the allocator has exclusive access to its resources
  Allocator::may_reasonably_fail;     // if true, the allocator may fail for reasons other than rare external factors such as being OOM
  Allocator::requires_deallocation;   // if false, avoiding deallocation is safe and/or doesn't cause memory leaks (ex, arena)
  Allocator::stateless;

  // if true, has 'reallocate' function taking pointer to old buffer and the parameters for a new allocation.
  // if the old buffer satisfies the requirements for the new allocation, just returns pointer to the old buffer.
  // otherwise, returns a pointer to a newly allocated buffer (but does not deallocate the old buffer).
  Allocator::supports_reallocate;
  Allocator::supports_allocate_raw;   // if true, has 'allocate_raw' function returning byte_t*
};

template <class Allocator>
concept raw_allocator_c = allocator_for_c<Allocator, byte_t> and requires {
  requires Allocator::supports_allocate_raw;
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
  { return std::start_lifetime_as_array<T>( (T*) ::operator new[](count * sizeof(T), alignment), count ); }

  eden_always_inline [[nodiscard]] static byte_t*
  allocate_raw(sz_t byte_count, align_t alignment) noexcept
  { return std::start_lifetime_as_array<byte_t>( (byte_t*) ::operator new[](byte_count, alignment), byte_count); }

  eden_always_inline static void
  deallocate(T* allocated, align_t allocated_alignment = TAlign) noexcept
  { ::operator delete[](allocated, allocated_alignment); }

  eden_always_inline static void
  deallocate(T* allocated, sz_t allocated_count, align_t allocated_alignment = TAlign) noexcept
  { ::operator delete[](allocated, allocated_count * sizeof(T), allocated_alignment); }

  eden_always_inline static void
  deallocate_raw(void* bytes) noexcept
  { ::operator delete[](bytes); }

  eden_always_inline static void
  deallocate_raw(void* bytes, sz_t byte_count) noexcept
  { ::operator delete[](bytes, byte_count); }

  eden_always_inline static void
  deallocate_raw(void* bytes, sz_t byte_count, align_t byte_alignment) noexcept
  { ::operator delete[](bytes, byte_count, byte_alignment); }

}; static_assert(allocator_for_c<BasicAllocator<int>, int>);

}