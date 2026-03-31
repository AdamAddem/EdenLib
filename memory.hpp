#pragma once
#include <cstddef>
#include <new>
#include <type_traits>

namespace eden {
template <class T> class allocator {
  using value_type = T;

public:
  constexpr allocator() noexcept = default;
  constexpr allocator(const allocator &other) noexcept = default;
  constexpr allocator(allocator &&other) noexcept = default;
  constexpr ~allocator() noexcept {}

  // returns nullptr on allocation failure
  constexpr T *allocate(std::size_t n) noexcept {
    return static_cast<T *>(operator new(n * sizeof(T), std::nothrow));
  }
  constexpr void
  deallocate(T *p, std::size_t n) noexcept(std::is_nothrow_destructible_v<T>) {
    operator delete(p, n);
  }
};

} // namespace eden
