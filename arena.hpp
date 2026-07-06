#pragma once
#include "typedefs.hpp"

#include <memory>
#include <new>
#include <vector>

namespace eden {

template <sz_t NBytes = 4096uz, sz_t Align = 64>
class Arena {
  void* end;
  sz_t remaining;

  static constexpr auto alloc_alignment = std::align_val_t{Align};
public:

  Arena() : remaining(NBytes) { end = ::operator new(NBytes, alloc_alignment); }
  Arena(Arena&& other) noexcept : end(other.end), remaining(other.remaining) { other.end = nullptr; other.remaining = 0; }

  // returns nullptr on failure
  template <class T>
  requires (sizeof(T) <= NBytes)
  [[nodiscard]] constexpr T*
  allocate(sz_t n = 1) noexcept {
    static constexpr sz_t Tsz = sizeof(T);
    static constexpr sz_t Talign = alignof(T);

    auto const alloc_bytes = Tsz * n;
    void* const new_alloc = std::align(Talign, alloc_bytes, end, remaining);
    if (new_alloc) {
      end = (char*)end + alloc_bytes;
      remaining -= alloc_bytes;
      return (T*) new_alloc;
    }

    remaining = 0;
    return nullptr;
  }

  constexpr void
  pop(sz_t bytes) noexcept {
    end = (char*)end - bytes;
    remaining += bytes;
  }

  ~Arena() {
    if (end == nullptr) return;
    ::operator delete( (char*)end - (NBytes - remaining), alloc_alignment );
  }

};

template <sz_t NBytes = 4096uz, sz_t Align = 64>
class ArenaPool {
  std::vector<Arena<NBytes, Align>> arenas;
public:

  ArenaPool() noexcept : arenas(1) {}

  template <class T>
  requires (sizeof(T) <= NBytes / 2) // why would you use an arena if you can only allocate one element?
  [[nodiscard]] T*
  allocate(sz_t count) noexcept {
    auto const res = arenas.back().template allocate<T>(count);
    if (res) return res;
    arenas.emplace_back();
    return arenas.back().template allocate<T>(count);
  }

  ArenaPool(ArenaPool const&) = delete;
  ArenaPool& operator=(ArenaPool const&) = delete;

  ArenaPool(ArenaPool&&) = delete;
  ArenaPool& operator=(ArenaPool&&) = delete;
};

}