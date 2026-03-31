#pragma once
#include "typedefs.hpp"
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace eden {

template <sz_t N>
class Bitset {
  using data_type = u64_t;
  static constexpr sz_t sizeofType = sizeof(data_type);
  static constexpr sz_t num_data = (N - 1) / sizeofType + 1;
  static constexpr data_type maxofType = std::numeric_limits<data_type>::max();

  [[nodiscard]] static constexpr sz_t
  indexOf(sz_t pos) noexcept
  {return pos / sizeofType;}

  [[nodiscard]] static constexpr data_type
  maskFor(sz_t pos) noexcept
  {return 1ull << (pos % sizeofType);}

  data_type bits[num_data];
public:

  constexpr Bitset() noexcept
  {std::memset(bits, 0, num_data * sizeofType);}

  explicit constexpr Bitset(const std::string& binary_string) noexcept
  : Bitset() {
    auto sz = binary_string.size();
    for (auto i{0uz}; i < N; ++i) {
      if (--sz == 0)
        return;
      const bool bit = (binary_string[sz] == '1');
      set(i, bit);
    }
  }

  constexpr Bitset(const Bitset &other) noexcept
  {std::memcpy(bits, other.bits, num_data * sizeofType);}

  [[nodiscard]] static constexpr sz_t
  size() noexcept
  {return N;}

  [[nodiscard]] constexpr bool
  operator[](sz_t pos) const noexcept
  {return test(pos);}

  [[nodiscard]] constexpr bool
  test(sz_t pos) const {
    if (indexOf(pos) > (num_data - 1))
      throw std::out_of_range(std::format("Element access at position {} in eden::bitset with size of {}.", pos, N));
    return bits[indexOf(pos)] & maskFor(pos);
  }

  [[nodiscard]] constexpr bool
  all() const noexcept {
    for (auto i{0uz}; i < N; ++i) {
      if (not test(i))
        return false;
    }
    return true;
  }

  [[nodiscard]] constexpr bool
  any() const noexcept {
    for (auto i{0uz}; i < N; ++i) {
      if (test(i))
        return true;
    }
    return false;
  }

  [[nodiscard]] constexpr bool
  none() const noexcept
  {return not any();}

  [[nodiscard]] constexpr sz_t
  count() const noexcept {
    sz_t num_true{};
    for (auto i{0uz}; i < N; ++i) {
      if (test(i))
        ++num_true;
    }
    return num_true;
  }

  friend constexpr Bitset operator bitand(const Bitset &lhs, const Bitset &rhs) noexcept {
    Bitset ret_set;
    for (auto i{0uz}; i < num_data; ++i)
      ret_set.bits[i] = lhs.bits[i] bitand rhs.bits[i];
    return ret_set;
  }

  friend constexpr Bitset operator bitor(const Bitset &lhs, const Bitset &rhs) noexcept {
    Bitset ret_set;
    for (auto i{0uz}; i < num_data; ++i)
      ret_set.bits[i] = lhs.bits[i] bitor rhs.bits[i];
    return ret_set;
  }

  friend constexpr Bitset operator xor(const Bitset &lhs, const Bitset &rhs) noexcept {
    Bitset ret_set;
    for (auto i{0uz}; i < num_data; ++i)
      ret_set.bits[i] = lhs.bits[i] xor rhs.bits[i];
    return ret_set;
  }

  constexpr Bitset&
  operator and_eq(const Bitset &other) noexcept
  {return *this = *this bitand other;}

  constexpr Bitset&
  operator or_eq(const Bitset &other) noexcept
  {return *this = *this bitor other;}

  constexpr Bitset&
  operator xor_eq(const Bitset &other) noexcept
  {return *this = *this xor other;}

  [[nodiscard]] constexpr Bitset
  operator~() const noexcept {
    Bitset ret_val = *this;
    ret_val.flip();
    return ret_val;
  }

  constexpr Bitset&
  set() noexcept {
    for (auto &num : bits)
      num = maxofType;
    return *this;
  }

  constexpr Bitset&
  set(sz_t pos, bool value = true) noexcept {
    if (value)
      bits[indexOf(pos)] or_eq maskFor(pos);
    else
      bits[indexOf(pos)] and_eq ~maskFor(pos);
    return *this;
  }

  constexpr Bitset&
  flip() noexcept {
    for (auto i{0uz}; i < N; ++i)
      set(i, not test(i));
    return *this;
  }

  constexpr Bitset&
  flip(sz_t pos) noexcept
  {return set(pos, not test(pos));}

  [[nodiscard]] constexpr std::string
  to_string() const noexcept {
    std::string ret_val;
    ret_val.reserve(N);

    //technically doesn't work if you want >9 quintibytes of data
    for (i64_t i{N - 1}; i >= 0; --i) {
      const char bit = test(i) ? '1' : '0';
      ret_val.push_back(bit);
    }

    return ret_val;
  }

};

}
