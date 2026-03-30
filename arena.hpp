#pragma once
#include "typedefs.hpp"
#include <iostream>
#include <memory>
#include <new>

namespace LOM {
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
  [[nodiscard]] T*
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

  ~Arena() {
    if (curr == nullptr)
      return;
    ::operator delete(static_cast<char*>(curr) - (N - remaining));
    delete next_arena;
  }

};
}