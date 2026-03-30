#pragma once
#include <cassert>

#define assume_assert(always_true) assert(always_true); [[assume(always_true)]]