#pragma once
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

using i8_t = std::int8_t;     static constexpr auto i8_max = std::numeric_limits<i8_t>::max();   static constexpr auto i8_min = std::numeric_limits<i8_t>::min();
using u8_t = std::uint8_t;    static constexpr auto u8_max = std::numeric_limits<u8_t>::max();   static constexpr auto u8_min = std::numeric_limits<u8_t>::min();

using i16_t = std::int16_t;   static constexpr auto i16_max = std::numeric_limits<i16_t>::max();  static constexpr auto i16_min = std::numeric_limits<i16_t>::min();
using u16_t = std::uint16_t;  static constexpr auto u16_max = std::numeric_limits<u16_t>::max();  static constexpr auto u16_min = std::numeric_limits<u16_t>::min();
using i32_t = std::int32_t;   static constexpr auto i32_max = std::numeric_limits<i32_t>::max();  static constexpr auto i32_min = std::numeric_limits<i32_t>::min();
using u32_t = std::uint32_t;  static constexpr auto u32_max = std::numeric_limits<u32_t>::max();  static constexpr auto u32_min = std::numeric_limits<u32_t>::min();
using i64_t = std::int64_t;   static constexpr auto i64_max = std::numeric_limits<i64_t>::max();  static constexpr auto i64_min = std::numeric_limits<i64_t>::min();
using u64_t = std::uint64_t;  static constexpr auto u64_max = std::numeric_limits<u64_t>::max();  static constexpr auto u64_min = std::numeric_limits<u64_t>::min();

using f32_t = float; static_assert(sizeof(float) == 4); static constexpr auto f32_max = std::numeric_limits<f32_t>::max(); static constexpr auto f32_min = std::numeric_limits<f32_t>::min();
using f64_t = double; static_assert(sizeof(double) == 8); static constexpr auto f64_max = std::numeric_limits<f64_t>::max(); static constexpr auto f64_min = std::numeric_limits<f64_t>::min();

using sz_t = std::size_t; static constexpr auto sz_max = std::numeric_limits<sz_t>::max(); static constexpr auto sz_min = std::numeric_limits<sz_t>::min();
using ssz_t = decltype(0z); static constexpr auto ssz_max = std::numeric_limits<ssz_t>::max(); static constexpr auto ssz_min = std::numeric_limits<ssz_t>::min();
using iptr_t = std::intptr_t;
using uptr_t = std::uintptr_t;

using byte_t = std::byte; static constexpr auto full_byte = byte_t{0xFF}; static constexpr auto empty_byte = byte_t{0x00};
using align_t = std::align_val_t;