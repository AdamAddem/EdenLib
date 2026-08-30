#pragma once
#include <cassert>
#include <type_traits>
#include <utility>

#define edenUnreachable(message) if(true) {assert(false and message); std::unreachable();} else (void)0
#define edenThrows(...) noexcept(false)

#ifdef __clang__
#define edenRestrict __restrict
#define edenAlwaysInline [[gnu::always_inline]]
#define edenNoInline [[gnu::noinline]]
#define edenCold [[gnu::cold]]
#define edenNoNullArgs [[gnu::nonnull]]
#define edenNonNullArgs(...) [[gnu::nonnull(__VA_ARGS__)]]
#define edenNotNullptr _Nonnull
#define edenReturnNonNull [[gnu::returns_nonnull]]
#elifdef __GNUG__
#define edenRestrict __restrict
#define edenAlwaysInline [[gnu::always_inline]]
#define edenNoInline [[gnu::noinline]]
#define edenCold [[gnu::cold]]
#define edenNoNullArgs [[gnu::nonnull]]
#define edenNonNullArgs(...) [[gnu::nonnull(__VA_ARGS__)]]
#define edenNotNullptr
#define edenReturnNonNull [[gnu::returns_nonnull]]
#elifdef _MSC_VER
#define edenRestrict __restrict
#define edenAlwaysInline [[msvc::forceinline]]
#define edenNoInline [[msvc::noinline]]
#define edenCold
#define edenNoNullArgs
#define edenNonNullArgs(...)
#define edenNotNullptr
#define edenReturnNonNull
#else
#define edenRestrict __restrict
#define edenAlwaysInline [[msvc::forceinline]]
#define edenNoInline [[msvc::noinline]]
#define edenCold
#define edenNoNullArgs
#define edenNonNullArgs(...)
#define edenNotNullptr
#define edenReturnNonNull
#endif

#if defined(__has_builtin)
  #if __has_builtin(__builtin_is_cpp_trivially_relocatable)
    #define edenTriviallyRelocatable(T) __builtin_is_cpp_trivially_relocatable(T)
  #elif __has_builtin(__is_trivially_relocatable)
    #define edenTriviallyRelocatable(T) __is_trivially_relocatable(T)
  #endif
#endif
#ifndef edenTriviallyRelocatable
  #define edenTriviallyRelocatable(T) std::is_trivially_move_constructible_v<T> and std::is_trivially_destructible_v<T>
#endif

#define edenNoInlineCold edenNoInline edenCold
#define edenInlineNodiscard edenAlwaysInline [[nodiscard]]
#define edenInlineNodiscardCXPR edenAlwaysInline [[nodiscard]] constexpr
#define edenInlineCXPR edenAlwaysInline [[nodiscard]]
#define edenNodiscardCXPR [[nodiscard]] constexpr
