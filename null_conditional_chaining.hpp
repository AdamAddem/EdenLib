#pragma once
#include "concepts.hpp"

namespace eden {
template <class T, class First, class... Remaining>
constexpr auto
call_methods_conditionally(T ptr, First first, Remaining... rest) noexcept(
    noexcept(call_methods_conditionally(first(ptr), rest...)))
    -> decltype(call_methods_conditionally(first(ptr), rest...))
  requires pointer_c<decltype(first(ptr))> {
  if (ptr)
    return call_methods_conditionally(first(ptr), rest...);
  if constexpr (pointer_c<decltype(call_methods_conditionally(first(ptr), rest...))>)
    return nullptr;
}

template <class T, class Last>
constexpr auto
call_methods_conditionally(T ptr, Last last) noexcept(noexcept(last(ptr)))
  -> decltype(last(ptr))
  requires pointer_c<decltype(last(ptr))> or void_c<decltype(last(ptr))>
{
  if (ptr)
    return last(ptr);

  if constexpr (pointer_c<decltype(last(ptr))>)
    return nullptr;
}

#define next_method(method_name, ...)                                          \
  [&]<class T>(T ptr) constexpr noexcept(noexcept((ptr->*&method_name)(        \
      __VA_ARGS__))) { return (ptr->*&method_name)(__VA_ARGS__); }

// use as such:
// call_methods_conditionally(init_ptr, next_method(T::first, args), next_method(B::second, args)... );

}
