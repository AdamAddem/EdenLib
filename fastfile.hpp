#pragma once
#include "typedefs.hpp"
#include "macros.hpp"
#include <system_error>
#include <type_traits>
#include <string_view>

#if defined(__linux__) || defined(__APPLE__)
#define IS_POSIX
#endif

#ifdef IS_POSIX
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#elifdef _WIN32
#include <Windows.h>
static_assert(false, "fastfile.hpp unsupported on your platform, sorry!");
#else
static_assert(false, "fastfile.hpp unsupported on your platform, sorry!");
#endif

namespace eden {

namespace detail {

#ifdef IS_POSIX
struct File {
  char const* text = nullptr;
  sz_t size = 0;

  static File open(cstr_t path) edenThrows(std::system_error) {
    File newfile;
    auto const fd = ::open(path, O_RDONLY);

    if(fd == -1)
      throw std::system_error(errno, std::system_category(), "Failed to open file");

    {
      struct stat stat;
      if(fstat(fd, &stat))
        ::close(fd), throw std::system_error(errno, std::system_category(), "Failed to stat file.");

      newfile.size = (sz_t) stat.st_size;
    }

    newfile.text = (char const*) mmap(nullptr, newfile.size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
    ::close(fd);
    if(newfile.text == MAP_FAILED)
      throw std::system_error(errno, std::system_category(), "Failed to mmap file.");

    return newfile;
  }

#define pre assert(file.isOpen());
  static void close(File&& file) noexcept { pre
    if( munmap((void*)file.text, file.size) ) assert(false), std::abort();

    file = {};
  }
#undef pre

  edenInlineNodiscard bool isOpen() const noexcept { return text == nullptr; }
};
#elifdef _WIN32
#endif

}

class [[nodiscard]] FastFile {
  using Fimpl = detail::File;
  Fimpl file;
public:
  edenInlineNodiscard std::string_view get() const noexcept { return {file.text, file.size}; }

  edenInlineCXPR FastFile() noexcept = default;

  edenAlwaysInline explicit FastFile(cstr_t path) edenThrows(std::system_error)
  : file(Fimpl::open(path)) {}

#define pre assert(not file.isOpen());
  void open(cstr_t path) edenThrows(std::system_error) { pre
    file = Fimpl::open(path);
  }
#undef pre

#define pre assert(file.isOpen());
  void close() noexcept { pre
    Fimpl::close(std::move(file));
  }
#undef pre

  edenAlwaysInline ~FastFile() {
    if(file.isOpen())
      Fimpl::close(std::move(file));
  }
};

}

#undef IS_POSIX