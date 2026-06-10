#pragma once
#include <array>
#include <algorithm>
#include <charconv>

#include "macros.hpp"
#include "typedefs.hpp"

namespace eden {

template<sz_t N>
struct TemplateString {
  std::array<char, N> data;
  consteval TemplateString(const char (&str)[N]) {
    std::copy_n(str, N, data.begin());
  }

  template<sz_t OtherN>
  consteval bool
  operator==(const TemplateString<OtherN>& other) const noexcept {
    if constexpr (N not_eq OtherN)
      return false;
    else
      return data == other.data;
  }

  static consteval sz_t size() noexcept {return N - 1;}
};

template<>
struct TemplateString<0> {
  std::array<char, 1> data{'\0'};
  static consteval sz_t size() noexcept {return 1;}
};

template <i64_t N, TemplateString str, sz_t str_size = str.data.size()>
[[nodiscard]] consteval auto append_number_to_literal_helper_func() noexcept
-> const char (&)[str_size+22] {
  struct scuffed {
    char arr[str_size+22]{};
    consteval scuffed() {
      std::copy_n(str.data.data(), str_size, arr);
      arr[str_size-1] = '<';
      auto end = std::to_chars(&arr[str_size], &arr[str_size+20], N).ptr;
      *end = '>';
      *(++end) = '\0';
    }
  };
  static constexpr scuffed x;
  return x.arr;
}

template <i64_t N, TemplateString str, sz_t str_size = str.data.size()>
static constexpr const char(&append_number_to_literal)[str_size+22] = append_number_to_literal_helper_func<N, str>();



// an implementation of POSIX's stpcpy
// returns a pointer to the end of the destination string
// assumes src and dest are pointing to different buffers
// assumes neither ptr is null
[[nodiscard]] constexpr char*
stpcpy_restrict(char* eden_restrict dest, const char* eden_restrict src) noexcept {
  assume_assert(dest); assume_assert(src);
  while (true) {
    *dest = *src;
    ++dest; ++src;
    if (*src == '\0')
      break;
  }

  return dest;
}

// an implementation of POSIX's stpcpy
// returns a pointer to the end of the destination string
// assumes neither ptr is null
[[nodiscard]] constexpr char*
stpcpy(char* dest, const char* src) noexcept {
  assume_assert(dest); assume_assert(src);
  while (true) {
    *dest = *src;
    ++dest; ++src;
    if (*src == '\0')
      break;
  }

  return dest;
}

[[nodiscard]] constexpr bool
streq_restrict(const char* eden_restrict first, const char* eden_restrict second, sz_t len) noexcept {
  assume_assert(first); assume_assert(second);
  auto i{0uz};
  while (i not_eq len) {
    if (first[i] not_eq second[i])
      return false;
    ++i;
  }
  return true;
}

[[nodiscard]] constexpr bool
streq_restrict(const char* eden_restrict first, const char* eden_restrict second) noexcept {
  assume_assert(first); assume_assert(second);

  auto i{0uz};
  while (true) {
    if (first[i] not_eq second[i])
      return false;
    if (first[i] == '\0')
      return true;
    ++i;
  }
}

[[nodiscard]] constexpr bool
streq(const char* first, const char* second, sz_t len) noexcept {
  auto i{0uz};
  while (i not_eq len) {
    if (first[i] not_eq second[i])
      return false;
    ++i;
  }
  return true;
}

[[nodiscard]] constexpr bool
streq(const char* first, const char* second) noexcept {
  auto i{0uz};
  while (true) {
    if (first[i] not_eq second[i])
      return false;
    if (first[i] == '\0')
      return true;
    ++i;
  }
}

}