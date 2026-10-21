#pragma once
#include <concepts>
#include "macros.hpp"
#include "typedefs.hpp"

namespace eden {

template <std::integral T>
edenConst edenInlineNodiscardCXPR
T narrow(std::integral auto val) {
  edenAssume( sz_t(val) == sz_t((T)val) );
  return (T) val;
}

}