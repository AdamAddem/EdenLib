#pragma once
#include <cstdint>

using i8_t = std::int8_t;
using u8_t = std::uint8_t;
using i16_t = std::int16_t;
using u16_t = std::uint16_t;
using i32_t = std::int32_t;
using u32_t = std::uint32_t;
using i64_t = std::int64_t;
using u64_t = std::uint64_t;
using f32_t = float; static_assert(sizeof(float) == 4);
using f64_t = double; static_assert(sizeof(double) == 8);

using sz_t = std::size_t;
using iptr_t = std::intptr_t;
using uptr_t = std::uintptr_t;
