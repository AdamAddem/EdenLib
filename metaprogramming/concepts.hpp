#pragma once
#include "../typedefs.hpp"
#include <memory>

namespace eden {

template <class Alloc, class T>
concept allocator_for_c = requires (Alloc a, T* p, sz_t n) {
  typename std::allocator_traits<Alloc>::value_type;
  typename std::allocator_traits<Alloc>::pointer;
  typename std::allocator_traits<Alloc>::const_pointer;
  typename std::allocator_traits<Alloc>::void_pointer;
  typename std::allocator_traits<Alloc>::const_void_pointer;
  typename std::allocator_traits<Alloc>::difference_type;
  typename std::allocator_traits<Alloc>::size_type;
  {std::allocator_traits<Alloc>::allocate(a, n)} -> std::same_as<typename std::allocator_traits<Alloc>::pointer>;
  std::allocator_traits<Alloc>::deallocate(a, p, n);
  Alloc(a);
};

template <class Pred, class T>
concept binary_predicate_for_c = std::invocable<Pred, T const&, T const&> and std::is_same_v<std::invoke_result_t<Pred, T const&, T const&>, bool>;

template <class T> concept pointer_c = std::is_pointer_v<T>;
template <class T> concept enum_c = std::is_enum_v<T>;
template <class T> concept void_c = std::is_void_v<T>;
template <class T, class U> concept same_c = std::is_same_v<T, U>;
template <class T, class U> concept different_c = not std::is_same_v<T, U>;
template <class From, class To> concept convertible_to_c = std::is_convertible_v<From, To>;

template <class T> concept default_constructible_c = std::is_default_constructible_v<T>;
template <class T> concept nothrow_default_constructible_c = std::is_nothrow_default_constructible_v<T>;
template <class T, class... Args> concept constructible_with_c = std::is_constructible_v<T, Args...>;
template <class T, class... Args> concept nothrow_constructible_with_c = std::is_nothrow_constructible_v<T, Args...>;
template <class T> concept copy_constructible_c = std::is_copy_constructible_v<T>;
template <class T> concept copy_assignable_c = std::is_copy_assignable_v<T>;
template <class T> concept nothrow_copy_constructible_c = std::is_nothrow_copy_constructible_v<T>;
template <class T> concept nothrow_copy_assignable_c = std::is_nothrow_copy_assignable_v<T>;
template <class T> concept move_constructible_c = std::is_move_constructible_v<T>;
template <class T> concept move_assignable_c = std::is_move_assignable_v<T>;
template <class T> concept nothrow_move_constructible_c = std::is_nothrow_move_constructible_v<T>;
template <class T> concept nothrow_move_assignable_c = std::is_nothrow_move_assignable_v<T>;
template <class T> concept swappable_c = std::swappable<T>;
template <class T> concept nothrow_swappable_c = std::is_nothrow_swappable_v<T>;

namespace detail {
  template<template <class...> class A, class... T> struct is_a : std::false_type {};
  template<template <class...> class A, class... T> struct is_a<A, A<T...>> : std::true_type {};
}

template <template <class...> class A, class... T>
concept is_a_c = detail::is_a<A, T...>::value;

template <class B, class C>
struct Test {};

static_assert(is_a_c< Test, Test<int, float> >);

}