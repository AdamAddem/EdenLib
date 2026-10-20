#pragma once

namespace eden {
  template <class, bool> struct ContainsIf {};
  template <class T>     struct ContainsIf<T, false> {};
  template <class T>     struct ContainsIf<T, true> {T m;};
}