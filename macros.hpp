#pragma once
#include <cassert>
#include <utility>
#include <type_traits>

#define eden_unreachable(message) if(true) {assert(false and message); std::unreachable();} else (void)0
#define eden_throws(...) noexcept(false)

#ifdef __clang__
#define eden_restrict __restrict
#define eden_always_inline [[gnu::always_inline]]
#define eden_noinline [[gnu::noinline]]
#define eden_cold [[gnu::cold]]
#define eden_nonull_args [[gnu::nonnull]]
#define eden_nonnull_args(...) [[gnu::nonnull(__VA_ARGS__)]]
#define eden_notnullptr _Nonnull
#define eden_return_nonnull [[gnu::returns_nonnull]]
#elifdef __GNUG__
#define eden_restrict __restrict
#define eden_always_inline [[gnu::always_inline]]
#define eden_noinline [[gnu::noinline]]
#define eden_cold [[gnu::cold]]
#define eden_nonull_args [[gnu::nonnull]]
#define eden_nonnull_args(...) [[gnu::nonnull(__VA_ARGS__)]]
#define eden_notnullptr
#define eden_return_nonnull [[gnu::returns_nonnull]]
#elifdef _MSC_VER
#define eden_restrict __restrict
#define eden_always_inline [[msvc::forceinline]]
#define eden_noinline [[msvc::noinline]]
#define eden_cold
#define eden_nonull_args
#define eden_nonnull_args(...)
#define eden_notnullptr
#define eden_return_nonnull

#else
#define eden_restrict
#define eden_always_inline
#define eden_noinline
#define eden_cold
#define eden_noinline_cold
#define eden_nonull_args
#define eden_nonnull_args(...)
#define eden_notnullptr
#define eden_return_nonnull
#endif

#if defined(__has_builtin)
  #if __has_builtin(__builtin_is_cpp_trivially_relocatable)
    #define eden_trivially_relocatable(T) __is_trivially_relocatable(T)
  #elif __has_builtin(__is_trivially_relocatable)
    #define eden_trivially_relocatable(T) __is_trivially_relocatable(T)
  #endif
#endif
#ifndef eden_trivially_relocatable
  #define eden_trivially_relocatable(T) std::is_trivially_move_constructible_v<T> and std::is_trivially_destructible_v<T>
#endif

#define eden_noinline_cold eden_noinline eden_cold
