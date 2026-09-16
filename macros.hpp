#pragma once
#include <cassert>
#include <type_traits>
#include <utility>

#define edenUnreachable(message) if(true) {assert(false and message); std::unreachable();} else (void)0
#define edenThrows(...) noexcept(false)
#define edenAssume(condition) if( not (condition) ) { edenUnreachable("Assumption Failed."); } else (void)0

#ifdef __clang__
#define edenRestrict __restrict
#define edenAlwaysInlineBASE [[gnu::always_inline]]
#define edenNoInline [[gnu::noinline]]
#define edenHot [[gnu::hot]]
#define edenCold [[gnu::cold]]
#define edenPure [[gnu::pure]]
#define edenConst [[gnu::const]]
#define edenNoNullArgs [[gnu::nonnull]]
#define edenNonNullArgs(...) [[gnu::nonnull(__VA_ARGS__)]]
#define edenNotNullptr _Nonnull
#define edenReturnNonNull [[gnu::returns_nonnull]]
#elifdef __GNUG__
#define edenRestrict __restrict
#define edenAlwaysInlineBASE [[gnu::always_inline]]
#define edenNoInline [[gnu::noinline]]
#define edenHot [[gnu::hot]]
#define edenCold [[gnu::cold]]
#define edenPure [[gnu::pure]]
#define edenConst [[gnu::const]]
#define edenNoNullArgs [[gnu::nonnull]]
#define edenNonNullArgs(...) [[gnu::nonnull(__VA_ARGS__)]]
#define edenNotNullptr
#define edenReturnNonNull [[gnu::returns_nonnull]]
#elifdef _MSC_VER
#define edenRestrict __restrict
#define edenAlwaysInlineBASE [[msvc::forceinline]]
#define edenNoInline [[msvc::noinline]]
#define edenHot
#define edenCold
#define edenPure
#define edenConst
#define edenNoNullArgs
#define edenNonNullArgs(...)
#define edenNotNullptr
#define edenReturnNonNull
#else
#define edenRestrict __restrict
#define edenAlwaysInlineBASE
#define edenNoInline
#define edenHot
#define edenCold
#define edenPure
#define edenConst
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

// might reduce compilation time in debug mode, idk
#ifndef NDEBUG
#undef edenAlwaysInlineBASE
#undef edenNoInline
#undef edenHot
#undef edenCold
#undef edenPure
#undef edenConst

#define edenAlwaysInlineBASE
#define edenNoInline
#define edenHot
#define edenCold
#define edenPure
#define edenConst
#endif


#define edenAlwaysInline edenAlwaysInlineBASE inline
#define edenNoInlineCold edenNoInline edenCold
#define edenInlineNodiscard edenAlwaysInlineBASE [[nodiscard]] inline
#define edenInlineNodiscardCXPR edenAlwaysInlineBASE [[nodiscard]] constexpr inline
#define edenInlineCXPR edenAlwaysInlineBASE constexpr inline
#define edenNodiscardCXPR [[nodiscard]] constexpr