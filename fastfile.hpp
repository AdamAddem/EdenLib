#pragma once
#include "typedefs.hpp"
#include "macros.hpp"
#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#elifdef _WIN32

#endif

namespace eden {

class FastFile {
  char const* text;
  sz_t len;
public:
  edenInlineNodiscard std::string_view get() const noexcept { return {text, len}; }
  
#if defined(__linux__) || defined(__APPLE__)

  FastFile(char const* path) {
    
  }


#elifdef _WIN32
#endif


};

}