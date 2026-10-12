#pragma once
#include "macros.hpp"

#define Unreachable(x) edenUnreachable(x)
#define Throws(...) edenThrows(__VA_ARGS__)
#define Assume(x) edenAssume(x)

#define Restrict edenRestrict
#define AlwaysInline edenAlwaysInline
#define InlineNodiscard edenInlineNodiscard
#define InlineCXPR edenInlineCXPR
#define InlineNodiscardCXPR edenInlineNodiscardCXPR
#define NodiscardCXPR edenNodiscardCXPR

#define NoInline edenNoInline
#define NoInlineCold edenNoInlineCold

#define HotFN edenHot
#define ColdFN edenCold
#define PureFN edenPure
#define ConstFN edenConst
#define NoNullArgs edenNoNullArgs
#define NonNullArgs(...) edenNonNullArgs(__VA_ARGS__)
#define NotNullptr edenNotNullptr
#define ReturnsNonNullptr edenReturnNonNullptr

#define TriviallyRelocatable(T) edenTriviallyRelocatable(T)