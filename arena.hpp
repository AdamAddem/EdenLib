#pragma once
#include "concepts.hpp"
#include "typedefs.hpp"

#include <memory>
#include <new>

namespace eden {

template <sz_t N = 4096uz>
class Arena {
  void* curr;
  sz_t remaining;
  Arena* next_arena{nullptr};
public:
  Arena() : remaining(N) {
    curr = ::operator new(N, std::align_val_t{8});
  }

  Arena(Arena&& other) noexcept
  : curr(other.curr), remaining(other.remaining), next_arena(other.next_arena) {
    other.curr = nullptr; other.next_arena = nullptr;
  }

  template <class T>
  [[nodiscard]] constexpr T*
  allocate() noexcept {
    if (next_arena)
      return next_arena->allocate<T>();

    void* const new_alloc = std::align(alignof(T), sizeof(T), curr,remaining);
    if (new_alloc) {
      curr = static_cast<char*>(curr) + sizeof(T);
      remaining -= sizeof(T);
      return static_cast<T*>(new_alloc);
    }

    next_arena = new Arena();
    return next_arena->allocate<T>();
  }

  constexpr void
  pop(sz_t bytes) noexcept {
    curr = static_cast<char*>(curr) - bytes;
    remaining += bytes;
  }

  ~Arena() {
    if (curr == nullptr)
      return;
    ::operator delete(static_cast<char*>(curr) - (N - remaining));
    delete next_arena;
  }

};

//if only user is false, deallocate will not do anything
//if only user is true, deallocate will just bump the pointer down by n
template <class T, sz_t N = 4096uz, bool OnlyUser = false>
class ArenaAllocator {
  Arena<N>* arena;
public:
  using value_type = T;

  [[nodiscard]] T*
  allocate(sz_t n) noexcept
  {return arena->template allocate<T>(n);}

  void deallocate([[maybe_unused]] T* p, sz_t n) noexcept
  {if constexpr (OnlyUser) arena->pop(n * sizeof(T));}

  [[nodiscard]] friend constexpr bool
  operator==(const ArenaAllocator&, const ArenaAllocator&) = default;
};

static_assert(allocator_for_c<ArenaAllocator<int>, int>);

}