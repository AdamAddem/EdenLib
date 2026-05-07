#pragma once
#include <cassert>
#include <utility>

#define assume_assert(always_true) [[assume((always_true))]]; assert((always_true))
#define eden_unreachable(message) assert(false and message), std::unreachable()

#ifdef __GNUG__
#define eden_restrict __restrict
#elif _MSC_VER
#define eden_restrict __restrict
#else
#define eden_restrict
#endif


#ifdef __clang__
#define eden_nonull_args [[gnu::nonnull]]
#define eden_nonnull_args(...) [[gnu::nonnull(__VA_ARGS__)]]
#define eden_notnullptr _Nonnull
#define eden_return_nonnull [[gnu::returns_nonnull]]

#elifdef __GNUG__
#define eden_nonull_args [[gnu::nonnull]]
#define eden_nonnull_args(...) [[gnu::nonnull(__VA_ARGS__)]]
#define eden_notnullptr
#define eden_return_nonnull [[gnu::returns_nonnull]]

#else
#define eden_nonull_args
#define eden_nonnull_args(...)
#define eden_notnullptr
#define eden_return_nonnull
#endif
