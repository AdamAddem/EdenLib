#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>

using i8_t = std::int8_t;     inline constexpr auto i8_max = std::numeric_limits<i8_t>::max();   inline constexpr auto i8_min = std::numeric_limits<i8_t>::min();
using u8_t = std::uint8_t;    inline constexpr auto u8_max = std::numeric_limits<u8_t>::max();   inline constexpr auto u8_min = std::numeric_limits<u8_t>::min();

using i16_t = std::int16_t;   inline constexpr auto i16_max = std::numeric_limits<i16_t>::max();  inline constexpr auto i16_min = std::numeric_limits<i16_t>::min();
using u16_t = std::uint16_t;  inline constexpr auto u16_max = std::numeric_limits<u16_t>::max();  inline constexpr auto u16_min = std::numeric_limits<u16_t>::min();
using i32_t = std::int32_t;   inline constexpr auto i32_max = std::numeric_limits<i32_t>::max();  inline constexpr auto i32_min = std::numeric_limits<i32_t>::min();
using u32_t = std::uint32_t;  inline constexpr auto u32_max = std::numeric_limits<u32_t>::max();  inline constexpr auto u32_min = std::numeric_limits<u32_t>::min();
using i64_t = std::int64_t;   inline constexpr auto i64_max = std::numeric_limits<i64_t>::max();  inline constexpr auto i64_min = std::numeric_limits<i64_t>::min();
using u64_t = std::uint64_t;  inline constexpr auto u64_max = std::numeric_limits<u64_t>::max();  inline constexpr auto u64_min = std::numeric_limits<u64_t>::min();

using f32_t = float; static_assert(sizeof(float) == 4); inline constexpr auto f32_max = std::numeric_limits<f32_t>::max(); inline constexpr auto f32_min = std::numeric_limits<f32_t>::min();
using f64_t = double; static_assert(sizeof(double) == 8); inline constexpr auto f64_max = std::numeric_limits<f64_t>::max(); inline constexpr auto f64_min = std::numeric_limits<f64_t>::min();

using sz_t = std::size_t; inline constexpr auto sz_max = std::numeric_limits<sz_t>::max(); inline constexpr auto sz_min = std::numeric_limits<sz_t>::min();
using iptr_t = std::intptr_t;
using uptr_t = std::uintptr_t;

using byte_t = std::byte; inline constexpr auto full_byte = byte_t{0xFF}; inline constexpr auto empty_byte = byte_t{0x00};