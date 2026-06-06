#pragma once
#include "metaprogramming.hpp"
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
  allocate(sz_t n = 1) noexcept {
    if (next_arena)
      return next_arena->allocate<T>(n);

    void* const new_alloc = std::align(alignof(T), sizeof(T) * n, curr,remaining);
    if (new_alloc) {
      curr = static_cast<char*>(curr) + sizeof(T) * n;
      remaining -= sizeof(T) * n;
      return static_cast<T*>(new_alloc);
    }

    next_arena = new Arena();
    return next_arena->allocate<T>(n);
  }

  constexpr void
  pop(sz_t bytes) noexcept {
    curr = static_cast<char*>(curr) - bytes;
    remaining += bytes;
  }

  ~Arena() {
    if (curr == nullptr)
      return;
    ::operator delete(static_cast<char*>(curr) - (N - remaining), std::align_val_t{8});
    delete next_arena;
  }

};

template <class T, sz_t N = 4096uz>
class ArenaAllocator {
  Arena<N>* arena;
public:
  using value_type = T;

  struct propagate_on_container_copy_assignment : std::true_type {};
  struct propagate_on_container_move_assignment : std::true_type {};
  struct propogate_on_container_swap : std::true_type{};

  [[nodiscard]] T*
  allocate(sz_t n) noexcept
  {return arena->template allocate<T>(n);}

  constexpr void
  deallocate([[maybe_unused]] T* p,[[maybe_unused]] sz_t n) noexcept
  {}

  [[nodiscard]] friend constexpr bool
  operator==(const ArenaAllocator&, const ArenaAllocator&) = default;

  [[nodiscard]] constexpr ArenaAllocator&
  operator=(const ArenaAllocator& other) noexcept = default;
};
static_assert(allocator_for_c<ArenaAllocator<int>, int>);

}