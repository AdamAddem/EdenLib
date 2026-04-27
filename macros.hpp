#pragma once
#include <cassert>

#define assume_assert(always_true) assert((always_true)); [[assume((always_true))]]

#ifdef __GNUC__
#define eden_restrict __restrict
#elif _MSC_VER
#define eden_restrict __restrict
#else
static_assert(false,
  "\nIf you are reading this, the compiler you are using does not support __restrict.\n"
  "This is fine, just know that some things may be less efficient.\n"
  "If your compiler does support some form of restrict, add it to the macro below.\n"
  "Delete this static assert to continue.");
#define eden_restrict
#endif