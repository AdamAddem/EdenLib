#pragma once
#include "typedefs.hpp"

#include <algorithm>
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

template<sz_t N>
struct TemplateString {
  std::array<char, N> data;
  consteval TemplateString(const char (&str)[N]) {
    std::copy_n(str, N, data.begin());
  }

  static consteval sz_t size() noexcept {return N;}
};


template<>
struct TemplateString<0> {
  std::array<char, 1> data{'\0'};
  static consteval sz_t size() noexcept {return 1;}
};

template <class T, TemplateString name_str = TemplateString<0>{}>
struct type {
  /* Traits */ #define trait static constexpr bool
  template <class U> trait is = std::same_as<T, U>;
  trait is_void = std::is_void_v<T>;
  trait is_nullptr = std::is_null_pointer_v<T>;
  trait is_integral = std::is_integral_v<T>;
  trait is_signed = std::is_signed_v<T>;
  trait is_unsigned = std::is_unsigned_v<T>;
  trait is_floating = std::is_floating_point_v<T>;
  trait is_scalar = std::is_scalar_v<T>;
  trait is_arithmetic = std::is_arithmetic_v<T>;
  trait is_primitive = std::is_fundamental_v<T>; trait is_fundamental = is_primitive;
  trait is_object = std::is_object_v<T>;
  trait is_compound = std::is_compound_v<T>;
  trait is_ptr = std::is_pointer_v<T>;
  trait is_ref = std::is_reference_v<T>;
  trait is_lvref = std::is_lvalue_reference_v<T>;
  trait is_rvref = std::is_rvalue_reference_v<T>;
  trait is_member_ptr = std::is_member_pointer_v<T>;
  trait is_field_ptr = std::is_member_object_pointer_v<T>; trait is_member_object_ptr = is_field_ptr;
  trait is_method_ptr = std::is_member_function_pointer_v<T>; trait is_member_method_ptr = is_method_ptr;
  trait is_const = std::is_const_v<T>;
  trait is_volatile = std::is_volatile_v<T>;
  trait is_array = std::is_array_v<T>;
  trait is_bounded_array = std::is_bounded_array_v<T>;
  trait is_unbounded_array = std::is_unbounded_array_v<T>;
  trait is_enum = std::is_enum_v<T>;
  trait is_enum_class = std::is_scoped_enum_v<T>;
  trait is_union = std::is_union_v<T>;
  trait is_class = std::is_class_v<T>;
  trait is_function = std::is_function_v<T>;
  trait is_trivial = std::is_trivial_v<T>;
  trait is_standard_layout = std::is_standard_layout_v<T>;
  trait has_unique_object_representation = std::has_unique_object_representations_v<T>;
  trait is_empty = std::is_empty_v<T>;
  trait is_polymorphic = std::is_polymorphic_v<T>;
  trait is_abstract = std::is_abstract_v<T>;
  trait is_final = std::is_final_v<T>;
  trait is_aggregate = std::is_aggregate_v<T>;

  template <class... Args> trait constructible_with = std::is_constructible_v<T, Args...>;
  template <class... Args> trait trivially_constructible_with = std::is_trivially_constructible_v<T, Args...>;
  template <class... Args> trait nothrow_constructible_with = std::is_nothrow_constructible_v<T, Args...>;
  template <class... Args> trait assignable_with = std::is_assignable_v<T, Args...>;
  template <class... Args> trait trivially_assignable_with = std::is_trivially_assignable_v<T, Args...>;
  template <class... Args> trait nothrow_assignable_with = std::is_nothrow_assignable_v<T, Args...>;
  trait default_constructible = std::is_default_constructible_v<T>;
  trait trivially_default_constructible = std::is_trivially_default_constructible_v<T>;
  trait nothrow_default_constructible = std::is_nothrow_default_constructible_v<T>;
  trait copy_constructible = std::is_copy_constructible_v<T>;
  trait trivially_copy_constructible = std::is_trivially_copy_constructible_v<T>;
  trait nothrow_copy_constructible = std::is_nothrow_copy_constructible_v<T>;
  trait copy_assignable = std::is_copy_assignable_v<T>;
  trait trivially_copy_assignable = std::is_trivially_copy_assignable_v<T>;
  trait nothrow_copy_assignable = std::is_nothrow_copy_assignable_v<T>;
  trait move_constructible = std::is_move_constructible_v<T>;
  trait trivially_move_constructible = std::is_trivially_move_constructible_v<T>;
  trait nothrow_move_constructible = std::is_nothrow_move_constructible_v<T>;
  trait move_assignable = std::is_move_assignable_v<T>;
  trait trivially_move_assignable = std::is_trivially_move_assignable_v<T>;
  trait nothrow_move_assignable = std::is_nothrow_move_assignable_v<T>;
  trait destructible = std::is_destructible_v<T>;
  trait trivially_destructible = std::is_trivially_destructible_v<T>;
  trait nothrow_destructible = std::is_nothrow_destructible_v<T>;
  trait has_virtual_destructor = std::has_virtual_destructor_v<T>;
  template <class U> trait swappable_with = std::swappable_with<T, U>;
  template <class U> trait nothrow_swappable_with = std::is_nothrow_swappable_with_v<T, U>;
  trait swappable = std::swappable<T>;
  trait nothrow_swappable = std::is_nothrow_swappable_v<T>;

  template <class U> trait same_as = std::same_as<T, U>;
  template <class U> trait base_of = std::is_base_of_v<T, U>;
  template <class U> trait derived_from = std::derived_from<T, U>;
  template <class U> trait convertible_to = std::convertible_to<T, U>;
  template <class U> trait nothrow_convertible_to = std::is_nothrow_convertible_v<T, U>;
  template <class U> trait layout_compatable = std::is_layout_compatible_v<T, U>;
  template <class... Args> trait invocable_with = std::is_invocable_v<T, Args...>;
  template <class Return, class... Args> trait invocable_r_with = std::is_invocable_r_v<Return, T, Args...>;
  template <class... Args> trait nothrow_invocable_with = std::is_nothrow_invocable_v<T, Args...>;
  template <class Return, class... Args> trait nothrow_invocable_r_with = std::is_nothrow_invocable_r_v<Return, T, Args...>;
  /* Traits */ #undef trait

  /* Properties */ #define property static constexpr auto
  static consteval sz_t alignment() {return alignof(T);}
  static consteval sz_t size() {return sizeof(T);}
  property num_dimensions = std::rank_v<T>; property rank = num_dimensions;
  property first_dimension_size = std::extent_v<T, 0>;
  property last_dimension_size = std::extent_v<T, u32_t(num_dimensions) - 1>;
  template<sz_t N> property dimension_size = std::extent_v<T, N>;
  /* Properties */ #undef property

  /* Type Transformations */
  using no_const = std::remove_const_t<T>;
  using as_const = std::add_const_t<T>;
  using no_volatile = std::remove_volatile_t<T>;
  using as_volatile = std::add_volatile_t<T>;
  using no_cv = std::remove_cv_t<T>;
  using as_cv = std::add_cv_t<T>;

  using no_ref = std::remove_reference_t<T>;
  using as_lvref = std::add_lvalue_reference_t<T>;
  using as_ref = as_lvref;
  using as_rvref = std::add_rvalue_reference_t<T>;
  using as_refref = as_rvref;
  using no_cvref = type<no_ref>::no_const;
  using as_const_lvref = const no_cvref&;
  using as_const_ref = as_const_lvref;
  using as_const_rvref = const no_cvref&&;
  using as_const_refref = as_const_rvref;

  using no_ptr = std::remove_pointer_t<T>;
  using pointed_type = no_ptr;
  using as_ptr = std::add_pointer_t<T>;

  template <sz_t N>
  using as_bounded_array = T[N];
  using as_array = T[];


private:
  template<typename K> struct opposite_sign_helper_struct{using type = K;};
  template<std::signed_integral K> struct opposite_sign_helper_struct<K> {using type = type<K>::as_unsigned;};
  template<std::unsigned_integral K> struct opposite_sign_helper_struct<K> {using type = type<K>::as_signed;};

  template<typename K> struct sign_helper_struct{using as_signed = K; using as_unsigned = K;};
  template<std::integral K> struct sign_helper_struct<K> {using as_signed = std::make_signed_t<K>; using as_unsigned = std::make_unsigned_t<K>;};
public:
  using as_signed = sign_helper_struct<T>::as_signed;
  using as_unsigned = sign_helper_struct<T>::as_unsigned;
  using opposite_sign = opposite_sign_helper_struct<T>::tstruct Example : type<Example> { int x; };
inline Example::no_volatile::as_bounded_array<5> x;

consteval void test() {
  x[0].x = 2;
}ype;

  using as_decayed = std::decay_t<T>;

  template <class K = T>
  requires std::is_enum_v<K>
  using underlying_type = std::underlying_type_t<K>;

  template<class... Types> using common_type_with = std::common_type_t<T, Types...>;

  template <class... Args> using invoke_result = std::invoke_result_t<T, Args...>;
  /* Type Transformations */

  /* Utilities */
  static constexpr const char* name = name_str.data.data();
  /* Utilities */
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
