#pragma once
#include "macros.hpp"
#include <type_traits>

namespace eden {

template <class T, class First, class... Remaining>
eden_always_inline constexpr auto call_methods_conditionally(T ptr, First first, Remaining... rest)
noexcept(noexcept(call_methods_conditionally(first(ptr), rest...)))
-> decltype(call_methods_conditionally(first(ptr), rest...))
requires std::is_pointer_v<decltype(first(ptr))> {
  if (ptr)
    return call_methods_conditionally(first(ptr), rest...);
  if constexpr (std::is_pointer_v<decltype(call_methods_conditionally(first(ptr), rest...))>)
    return nullptr;
}

template <class T, class Last>
eden_always_inline constexpr auto call_methods_conditionally(T ptr, Last last) noexcept(noexcept(last(ptr)))
-> decltype(last(ptr))
requires std::is_pointer_v<decltype(last(ptr))> or std::is_void_v<decltype(last(ptr))> {
  if (ptr)
    return last(ptr);

  if constexpr (std::is_pointer_v<decltype(last(ptr))>)
    return nullptr;
}

#define next_method(method_name, ...)                                          \
  [&]<class T>(T ptr) \
  constexpr noexcept(noexcept((ptr->*&method_name)(__VA_ARGS__))) { \
    return (ptr->*&method_name)(__VA_ARGS__); \
  }

// use as such:
// call_methods_conditionally(init_ptr, next_method(T::first, first_args), next_method(B::second, second_args)... );

}
