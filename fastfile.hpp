#pragma once
#include "typedefs.hpp"
#include "macros.hpp"
#include <system_error>
#include <type_traits>

#if defined(__linux__) || defined(__APPLE__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#elifdef _WIN32

#endif

namespace eden {

namespace detail {
#if defined(__linux__) || defined(__APPLE__)
template <bool MaybeLarge = true>
struct File {
  char const* text;
  std::conditional_t<MaybeLarge, sz_t, u32_t> size;
  int fd;

  static File open(char const* path) edenThrows(std::system_error) {
    File newfile;
    newfile.fd = ::open(path, O_RDONLY);

    if(newfile.fd == -1)
      throw std::system_error(errno, std::system_category(), "Failed to open file");

    {
      struct stat stat;
      if(fstat(newfile.fd, &stat))
        ::close(newfile.fd), throw std::system_error(errno, std::system_category(), "Failed to stat file.");

      newfile.size = (sz_t) stat.st_size;
    }

    newfile.text = (char const*) mmap(nullptr, newfile.size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, newfile.fd, 0);
    if(newfile.text == MAP_FAILED)
      ::close(newfile.fd), throw std::system_error(errno, std::system_category(), "Failed to mmap file.");


    return newfile;
  }
  static void close(File&& file) noexcept {
    assert(file.isOpen());
    if( ::close(file.fd) ) assert(false), std::abort();
    if( munmap((void*)file.text, file.size) ) assert(false), std::abort();

    file.text = nullptr;
    file.size = 0;
    file.fd = -1;
  }

  edenInlineNodiscard bool isOpen() noexcept { return text == nullptr; }
};

#elifdef _WIN32

#endif

}

template <bool MaybeLarge = false>
class [[nodiscard]] FastFile {
  using Fimpl = detail::File<MaybeLarge>;
  Fimpl file;
public:
  edenInlineNodiscard std::string_view get() const noexcept { return {file.text, file.size}; }

  edenAlwaysInline FastFile(char const* path) edenThrows(std::system_error)
  : file(Fimpl::open(path)) {
    try { throw 0; } catch(...) {}
  }


  edenAlwaysInline ~FastFile() {
    if(file.isOpen())
      Fimpl::close(std::move(file));
  }
};

}
