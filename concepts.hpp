#pragma once
#include "typedefs.hpp"
#include "string_utils.hpp"

#include <array>
#include <concepts>
#include <cstring>
#include <memory>
#include <type_traits>

namespace eden {
template <class Alloc, class T>
concept allocator_for_c = requires (Alloc a, T* p, std::size_t n) {
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

//Forward Tuple implementation was shamelessly stolen from someone on stackoverflow
//Thank you!
namespace detail {
    template <std::size_t Index, typename T>
    class TupleElement {
        T value;
    protected:
        TupleElement(const T& val) : value(val) {}

        template <std::size_t I>
        constexpr std::enable_if_t<I == Index, T&>
        get() { return value; }

        template <std::size_t I>
        constexpr std::enable_if_t<I == Index, const T&>
        get() const { return value; }
    };

    template <typename Ind, typename... Ts>
    class TupleImpl;

    template <std::size_t... Is, typename... Ts>
    class TupleImpl<std::index_sequence<Is...>, Ts...> : TupleElement<Is, Ts>... {
    public:
        constexpr TupleImpl(const Ts&... args)
        : TupleElement<Is, Ts>(args)... {}
        using TupleElement<Is, Ts>::get...;
    };
}

template <typename... Ts>
struct ForwardTuple : detail::TupleImpl<std::index_sequence_for<Ts...>, Ts...> {
    using detail::TupleImpl<std::index_sequence_for<Ts...>, Ts...>::TupleImpl;
};

template <typename... Ts>
ForwardTuple(const Ts&...) -> ForwardTuple<Ts...>;



template <class T, TemplateString name_str = TemplateString<0>{}>
struct type {
  using Type = T;
  static consteval auto type_instance() {return type{};}

  template <class OtherType, TemplateString other_name>
  consteval bool
  operator==(const type<OtherType, other_name>&) const noexcept {
    if constexpr (std::is_same_v<Type, OtherType>)
      return true;
    else
      return false;
  }

  /* Traits */
  #define trait(trait_name, ...) \
  static constexpr bool trait_name = __VA_ARGS__; \
  static consteval bool _ ## trait_name () {return trait_name;}

  #define template_trait(template_param, trait_name, ...) \
  template <class template_param> \
  static constexpr bool trait_name = __VA_ARGS__; \
  template <class template_param> \
  static consteval bool _ ## trait_name (type<template_param>) {return trait_name<template_param>;}

  #define va_template_trait(args_name, trait_name, ...) \
  template <class... args_name> \
  static constexpr bool trait_name = __VA_ARGS__; \
  template <class... args_name> \
  static consteval bool _ ## trait_name (type<args_name>...) {return trait_name<args_name...>;}

  trait(is_void, std::is_void_v<T>)
  trait(is_nullptr, std::is_null_pointer_v<T>)
  trait(is_integral, std::is_integral_v<T>)
  trait(is_signed, std::is_signed_v<T>)
  trait(is_unsigned, std::is_unsigned_v<T>)
  trait(is_floating, std::is_floating_point_v<T>)
  trait(is_scalar, std::is_scalar_v<T>)
  trait(is_arithmetic, std::is_arithmetic_v<T>)
  trait(is_primitive, std::is_fundamental_v<T>) trait(is_fundamental, is_primitive)
  trait(is_object, std::is_object_v<T>)
  trait(is_compound, std::is_compound_v<T>)
  trait(is_ptr, std::is_pointer_v<T>)
  trait(is_ref, std::is_reference_v<T>)
  trait(is_lvref, std::is_lvalue_reference_v<T>)
  trait(is_rvref, std::is_rvalue_reference_v<T>)
  trait(is_member_ptr, std::is_member_pointer_v<T>)
  trait(is_field_ptr, std::is_member_object_pointer_v<T>) trait(is_member_object_ptr, is_field_ptr)
  trait(is_method_ptr, std::is_member_function_pointer_v<T>) trait(is_member_method_ptr, is_method_ptr)
  trait(is_const, std::is_const_v<T>)
  trait(is_volatile, std::is_volatile_v<T>)
  trait(is_array, std::is_array_v<T>)
  trait(is_bounded_array, std::is_bounded_array_v<T>)
  trait(is_unbounded_array, std::is_unbounded_array_v<T>)
  trait(is_enum, std::is_enum_v<T>)
  trait(is_enum_class, std::is_scoped_enum_v<T>)
  trait(is_union, std::is_union_v<T>)
  trait(is_class, std::is_class_v<T>)
  trait(is_function, std::is_function_v<T>)
  trait(is_trivial, std::is_trivial_v<T>)
  trait(is_standard_layout, std::is_standard_layout_v<T>)
  trait(has_unique_object_representation, std::has_unique_object_representations_v<T>)
  trait(is_empty, std::is_empty_v<T>)
  trait(is_polymorphic, std::is_polymorphic_v<T>)
  trait(is_abstract, std::is_abstract_v<T>)
  trait(is_final, std::is_final_v<T>)
  trait(is_aggregate, std::is_aggregate_v<T>)

  va_template_trait(Args, constructible_with, comma(std::is_constructible_v<T, Args...>))
  va_template_trait(Args, trivially_constructible_with , comma(std::is_trivially_constructible_v<T, Args...>))
  va_template_trait(Args, nothrow_constructible_with , comma(std::is_nothrow_constructible_v<T, Args...>))
  va_template_trait(Args, assignable_with , comma(std::is_assignable_v<T, Args...>))
  va_template_trait(Args, trivially_assignable_with , comma(std::is_trivially_assignable_v<T, Args...>))
  va_template_trait(Args, nothrow_assignable_with , comma(std::is_nothrow_assignable_v<T, Args...>))

  trait(default_constructible,  std::is_default_constructible_v<T>)
  trait(trivially_default_constructible,  std::is_trivially_default_constructible_v<T>)
  trait(nothrow_default_constructible,  std::is_nothrow_default_constructible_v<T>)
  trait(copy_constructible,  std::is_copy_constructible_v<T>)
  trait(trivially_copy_constructible,  std::is_trivially_copy_constructible_v<T>)
  trait(nothrow_copy_constructible,  std::is_nothrow_copy_constructible_v<T>)
  trait(copy_assignable,  std::is_copy_assignable_v<T>)
  trait(trivially_copy_assignable,  std::is_trivially_copy_assignable_v<T>)
  trait(nothrow_copy_assignable,  std::is_nothrow_copy_assignable_v<T>)
  trait(move_constructible,  std::is_move_constructible_v<T>)
  trait(trivially_move_constructible,  std::is_trivially_move_constructible_v<T>)
  trait(nothrow_move_constructible,  std::is_nothrow_move_constructible_v<T>)
  trait(move_assignable,  std::is_move_assignable_v<T>)
  trait(trivially_move_assignable,  std::is_trivially_move_assignable_v<T>)
  trait(nothrow_move_assignable,  std::is_nothrow_move_assignable_v<T>)
  trait(destructible,  std::is_destructible_v<T>)
  trait(trivially_destructible,  std::is_trivially_destructible_v<T>)
  trait(nothrow_destructible,  std::is_nothrow_destructible_v<T>)
  trait(has_virtual_destructor,  std::has_virtual_destructor_v<T>)

  trait(swappable, std::swappable<T>)
  trait(nothrow_swappable, std::is_nothrow_swappable_v<T>)
  template_trait(U, swappable_with, std::swappable_with<T, U>)
  template_trait(U, nothrow_swappable_with, std::is_nothrow_swappable_with_v<T, U>)
  template_trait(U, is, std::same_as<T, U>)
  template_trait(U, base_of, std::is_base_of_v<T, U>)
  template_trait(U, derived_from, std::derived_from<T, U>)
  template_trait(U, convertible_to, std::convertible_to<T, U>)
  template_trait(U, nothrow_convertible_to, std::is_nothrow_convertible_v<T, U>)
  template_trait(U, layout_compatable, std::is_layout_compatible_v<T, U>)

  va_template_trait(Types, is_one_of, (is<Types> or ...))
  va_template_trait(Types, is_none_of, not is_one_of<Types...>)
  va_template_trait(Types, is_only_one_of, (is<Types> + ...) == 1)
  va_template_trait(Args, invocable_with, std::is_invocable_v<T, Args...>)
  va_template_trait(Args, nothrow_invocable_with, std::is_nothrow_invocable_v<T, Args...>)

  template <class Return, class... Args>
  static constexpr bool invocable_r_with = std::is_invocable_r_v<Return, T, Args...>;
  template <class Return, class... Args>
  static consteval bool _invocable_r_with(type<Return>, type<Args>...) {return invocable_r_with<Return, Args...>;}

  template <class Return, class... Args>
  static constexpr bool nothrow_invocable_r_with = std::is_nothrow_invocable_r_v<Return, T, Args...>;
  template <class Return, class... Args>
  static consteval bool _nothrow_invocable_r_with(type<Return>, type<Args>...) {return nothrow_invocable_r_with<Return, Args...>;}

  #undef trait
  #undef template_trait
  #undef va_template_trait
  /* Traits */

  /* Properties */ #define property static constexpr sz_t
  property alignment() {return alignof(T);}
  property size() {return sizeof(T);}
  property num_dimensions = std::rank_v<T>; property rank = num_dimensions;
  property first_dimension_size = std::extent_v<T, 0>;
  property last_dimension_size = std::extent_v<T, u32_t(num_dimensions) - 1>;
  template<sz_t N> property dimension_size = std::extent_v<T, N>;
  /* Properties */ #undef property

  /* Type Transformations */
#define chain_type(transform_name, ...) \
  using transform_name = __VA_ARGS__ ; \
  using transform_name ## _ = type<transform_name, name_str>; \
  static constexpr auto transform_name ## _i() {return type<transform_name, name_str>{};}

#define template_chain_type(template_, template_param, transform_name, ...) \
  template_ \
  using transform_name = __VA_ARGS__; \
  template_ \
  using transform_name ## _ = type<transform_name<template_param>, name_str>; \
  template_ \
  static constexpr auto transform_name ## _i() {return type<transform_name<template_param>, name_str>{};}

#define va_template_chain_type(template_param, transform_name, ...) \
  template<class... template_param> \
  using transform_name = __VA_ARGS__ ; \
  template<class... template_param> \
  using transform_name ## _ = type<transform_name<template_param...>, name_str>; \
  template<class... template_param> \
  static constexpr auto transform_name ## _i(){return type<transform_name<template_param...>, name_str>{};}

private:
  template<typename K> struct opposite_sign_helper_struct{using type = K;};
  template<std::signed_integral K> struct opposite_sign_helper_struct<K> {using type = type<K, name_str>::as_unsigned;};
  template<std::unsigned_integral K> struct opposite_sign_helper_struct<K> {using type = type<K, name_str>::as_signed;};

  template<typename K> struct sign_helper_struct{using as_signed = K; using as_unsigned = K;};
  template<std::integral K> struct sign_helper_struct<K> {using as_signed = std::make_signed_t<K>; using as_unsigned = std::make_unsigned_t<K>;};

public:
  chain_type(no_const, std::remove_const_t<T>)
  chain_type(as_const, const T)
  chain_type(no_volatile, std::remove_volatile_t<T>)
  chain_type(as_volatile , std::add_volatile_t<T>)
  chain_type(no_cv , std::remove_cv_t<T>)
  chain_type(as_cv , std::add_cv_t<T>)
  chain_type(no_ref, std::remove_reference_t<T>)

  chain_type(as_lvref, std::add_lvalue_reference_t<T>) chain_type(as_ref, as_lvref)
  chain_type(as_rvref, std::add_rvalue_reference_t<T>) chain_type(as_refref, as_rvref)
  chain_type(no_cvref, type<no_ref, name_str>::no_cv)
  chain_type(as_const_lvref, const no_cvref&)
  chain_type(as_const_ref, as_const_lvref)
  chain_type(as_const_rvref, const no_cvref&&)
  chain_type(as_const_refref, as_const_rvref)

  template_chain_type(template <sz_t N>, N, as_bounded_array, T[N])

  chain_type(no_ptr, std::remove_pointer_t<T>)
  chain_type(as_ptr, type<no_ref, name_str>::Type *)
  chain_type(as_array, T[])
  chain_type(as_signed, sign_helper_struct<T>::as_signed)
  chain_type(as_unsigned, sign_helper_struct<T>::as_unsigned)
  chain_type(opposite_sign, opposite_sign_helper_struct<T>::type)
  chain_type(as_decayed, std::decay_t<T>)

  template_chain_type(
    template <class K = T> requires std::is_enum_v<K>,
    K,
    underlying_type,
    std::underlying_type_t<K>)

  va_template_chain_type(
    Types,
    common_type_with,
    std::common_type_t<T, Types...>)

  va_template_chain_type(
    Args,
    invoke_result,
    std::invoke_result_t<T, Args...>)

  #undef chain_type
  #undef template_chain_type
  #undef va_template_chain_type
  /* Type Transformations */

  /* Utilities */
  static constexpr const char* name = name_str.data.data();
  /* Utilities */
};

template <class T>
concept registered_type = std::derived_from<T, type<T>> or
  requires (T a) {
    []<TemplateString N>(type<T, N>){}(a);
  };

template <class T>
concept type_instance = requires (T a) {
  []<typename U, TemplateString str>(type<U, str>) consteval {
  }(a);
};



template <class T>
extern T extern_never_defined;

template <typename T>
union UnionWithCharArray {
  alignas(T) char a[sizeof(T)];
  T t;
};

template <sz_t I, class... Args>
static constexpr void assign_offsets(sz_t* item) noexcept {
  if constexpr (I < sizeof...(Args)) {
    using tuple_type = ForwardTuple<Args...>;
    *item = [] {
      auto& o = extern_never_defined<UnionWithCharArray<tuple_type>>;
      for (sz_t i = 0;; ++i)
        if (std::addressof(o.t.template get<I>()) == static_cast<void*>(o.a + i))
          return i;
    }();

    assign_offsets<I + 1, Args...>(item + 1);
  }
}

template<class... Args>
static constexpr auto offsets() noexcept -> const sz_t(&)[sizeof...(Args)] {
  struct Bullshit {
    sz_t arr[sizeof...(Args)];

    consteval Bullshit() {
      assign_offsets<0, Args...>(arr);
    }
  };

  static constexpr Bullshit offsets;
  return offsets.arr;
}

template <class T, TemplateString name_str, class... Members>
struct class_reflection {
  static constexpr sz_t N = sizeof...(Members);
  static constexpr const char* name = name_str.data.data();
  using type = T;
  using tuple_type = ForwardTuple<typename Members::type...>;

  static consteval auto member_names() -> const char* const (&)[N]{
    static constexpr const char *arr[N] = {Members::name... };
    return arr;
  }
  static consteval auto type_offsets() {return offsets<typename Members::type...>();}

  template <sz_t I>
  static constexpr void for_each_h(const char* obj, auto&& callable) {
    if constexpr(I < sizeof...(Members)) {
      using element_type = std::remove_reference_t<decltype(extern_never_defined<tuple_type>.template get<I>())>;
      callable(
        member_names()[I],
        *reinterpret_cast<const element_type*>(obj + type_offsets()[I])
        );
      for_each_h<I + 1>(obj, std::forward<decltype(callable)>(callable));
    }
  }

  static constexpr void for_each(const T& obj, auto&& callable) {
    const auto ptr = reinterpret_cast<const char*>(std::addressof(obj));
    for_each_h<0>(ptr, std::forward<decltype(callable)>(callable));
  }
};



template <class T> concept pointer_c = std::is_pointer_v<T>;
template <class T> concept enum_c = std::is_enum_v<T>;
template <class T> concept void_c = std::is_void_v<T>;
template <class T, class U> concept same_c = std::is_same_v<T, U>;
template <class T, class U> concept different_c = not std::is_same_v<T, U>;

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


}
