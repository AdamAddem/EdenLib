#pragma once
#include "basic_allocator.hpp"
#include <vector>

namespace eden {
// Exclusive dictates whether the owner of the arena has exclusive ownership over the data
template <sz_t NBytes = 4096, align_t ArenaAlignment = align_t{64}, bool Exclusive = false>
class Arena {
  inline static constinit byte_t invalid_end[1]{};

  byte_t* end;
  sz_t remaining;

public:

  using value_type = byte_t;
  static constexpr bool exclusive_use = Exclusive;
  static constexpr bool may_reasonably_fail = true;
  static constexpr bool requires_deallocation = false;
  static constexpr bool stateless = false;
  static constexpr bool supports_allocate_raw = true;
  static constexpr bool supports_reallocate = true;

  edenAlwaysInline Arena() noexcept : end(std::start_lifetime_as_array<byte_t>((byte_t*)::operator new(NBytes, ArenaAlignment), NBytes)), remaining(NBytes) {}
  edenAlwaysInline Arena(Arena&& other) noexcept : end(other.end), remaining(other.remaining) { other.end = invalid_end; other.remaining = 0; }
  edenAlwaysInline ~Arena() { 
    if(end not_eq invalid_end) ::operator delete( end - (NBytes - remaining), ArenaAlignment );
  }

  // second parameter is not used and will always align to alignof(T)
  template <class T = byte_t>
  requires (sizeof(T) <= NBytes)
  [[nodiscard]] constexpr T*
  allocate(sz_t count, align_t alignment = (align_t)alignof(T)) noexcept {
    auto const alloc_bytes = count * sizeof(T);

    void* tmp = end;
    auto const new_alloc = std::align( (sz_t) alignment, alloc_bytes, tmp, remaining);
    end = (byte_t*)tmp;
    if (new_alloc) {
      end = end + alloc_bytes;
      remaining -= alloc_bytes;
      return std::start_lifetime_as_array<T>((T*) new_alloc, count);
    }

    return nullptr;
  }

  edenAlwaysInline [[nodiscard]] constexpr byte_t*
  allocate_raw(sz_t byte_count, align_t alignment) noexcept 
  { return allocate<byte_t>(byte_count, alignment); }

  // returns old_buff if it is the most recent allocation and the arena holds enough storage for the extra elements
  // otherwise, returns a new allocation as if by allocate(new_count)
  // old_buff MUST be from this arena, and old_count MUST reflect that allocations 'count' parameter
  template <class T>
  requires (sizeof(T) <= NBytes)
  [[nodiscard]] constexpr T*
  reallocate(T* old_buff, sz_t old_count, sz_t new_count, align_t = {}) noexcept {
    if( (byte_t*)(old_buff + old_count) not_eq end )
      return allocate<T>(new_count);

    auto const old_allocation_bytes = old_count * sizeof(T);
    auto const new_allocation_bytes = new_count * sizeof(T);
    auto const old_remaining = remaining + old_allocation_bytes;
    if( new_allocation_bytes > old_remaining )
      return nullptr;

    end = ((byte_t*)old_buff) + new_allocation_bytes;
    remaining = old_remaining - new_allocation_bytes;

    // might be UB? if it was allocated previously then old_buff should already have a lifetime of an array of old_count.
    // not sure if this conflicts w/ that.
    return std::start_lifetime_as_array<T>(old_buff, new_count);
  }

  edenAlwaysInline static void deallocate(void*, align_t = {})       noexcept {}
  edenAlwaysInline static void deallocate(void*, sz_t, align_t = {}) noexcept {}
  edenAlwaysInline static void deallocate_raw(void*, align_t = {})       noexcept {}
  edenAlwaysInline static void deallocate_raw(void*, sz_t, align_t = {}) noexcept {}
  
}; static_assert(raw_allocator_c< Arena<> >);


template <sz_t BytesPerArena = 4096, align_t ArenaAlignment = align_t{64}, bool Exclusive = false>
class ArenaPool {
  std::vector< Arena<BytesPerArena, ArenaAlignment, true> > arenas;
public:

  using value_type = byte_t;
  static constexpr bool exclusive_use = Exclusive;
  static constexpr bool may_reasonably_fail = false;
  static constexpr bool requires_deallocation = false;
  static constexpr bool stateless = false;
  static constexpr bool supports_allocate_raw = true;
  static constexpr bool supports_reallocate = false;

  ArenaPool() noexcept : arenas(1) {}

  template <class T = byte_t>
  requires (sizeof(T) <= BytesPerArena)
  [[nodiscard]] constexpr T*
  allocate(sz_t count, align_t alignment = align_t{alignof(T)}) noexcept {
    assert(count * sizeof(T) <= BytesPerArena);
    auto res = arenas.back().template allocate<T>(count, alignment);
    if (res) return res;
    res = arenas.emplace_back().template allocate<T>(count, alignment);
    assert(res);
    return res;
  }

  edenAlwaysInline [[nodiscard]] constexpr byte_t*
  allocate_raw(sz_t byte_count, align_t alignment) noexcept {
    assert(byte_count <= BytesPerArena);
    auto res = arenas.back().template allocate<byte_t>(byte_count, alignment);
    if (res) return res;
    res = arenas.emplace_back().template allocate<byte_t>(byte_count, alignment);
    assert(res);
    return res;   
  }

  edenAlwaysInline static constexpr void deallocate(void*, align_t = {})       noexcept {}
  edenAlwaysInline static constexpr void deallocate(void*, sz_t, align_t = {}) noexcept {}
  edenAlwaysInline static constexpr void deallocate_raw(void*, align_t = {})       noexcept {}
  edenAlwaysInline static constexpr void deallocate_raw(void*, sz_t, align_t = {}) noexcept {}

  ArenaPool(ArenaPool const&) = delete;
  ArenaPool(ArenaPool&&) = delete;
  ArenaPool& operator=(ArenaPool const&) = delete;
  ArenaPool& operator=(ArenaPool&&) = delete;
}; static_assert(raw_allocator_c< ArenaPool<> >);
}