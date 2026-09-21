#pragma once
#include "../metaprogramming/packs.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"

#include <cstring>
#include <utility>
#include <span>
#include <tuple>
#include <new>
#include <type_traits>
#include <cassert>

namespace eden {

namespace detail {

template<class... Ts>
class Slices {
  static constexpr packs::PackData<Ts...> pack_data{};
  static constexpr auto buffer_begin_idx = pack_data.alignidx_to_typeidx[0];
  std::tuple<Ts*...> ptrs{};
  
  template <sz_t idx, class First, class... Rest>
  edenInlineCXPR void
  destroy_all_impl(sz_t size_each) noexcept {
    if(size_each == 0) return;
    if constexpr(sizeof...(Rest)) destroy_all_impl<idx + 1, Rest...>(size_each);
  
    if constexpr(not std::is_trivially_destructible_v<First>) {
      auto const begin = get<idx>();
      auto end = begin + size_each - 1;
      while(true) {
        std::destroy_at(end);
        if(end == begin) break;
        end -= 1;
      }
    }
  }

  template<sz_t idx, class First, class... Rest>
  edenInlineCXPR void 
  reslice_with_impl(byte_t* alloc, sz_t new_capacity_each) noexcept {
    auto const slice = alloc + pack_data.alignidx_to_offset[ pack_data.typeidx_to_alignidx[idx] ];
    get<idx>() = std::start_lifetime_as_array<First>(slice, new_capacity_each);
    if constexpr(sizeof...(Rest))
      return reslice_with_impl<idx + 1, Rest...>(alloc, new_capacity_each);
  }
  
  template <sz_t idx, class First, class... Rest>
  edenInlineCXPR void
  relocate_impl(Slices new_slices, sz_t size_each) noexcept {
    auto old_ptr = get<idx>();
    auto construct_location = new_slices.get<idx>();
  
    if constexpr(edenTriviallyRelocatable(First))
      std::memcpy( construct_location, old_ptr, size_each * sizeof(First) );
    else
      for(auto i{0uz}; i<size_each; ++i)
        std::construct_at( construct_location + i, std::move(old_ptr[i]) ), 
        std::destroy_at( old_ptr + i ); // technically not reverse order but who really cares
  
    if constexpr(sizeof...(Rest))
      relocate_impl<idx + 1, Rest...>(new_slices, size_each);
  }
  
  template <sz_t idx, class First, class... Rest>
  edenInlineCXPR void
  add_to_end_unchecked_impl(sz_t size_each, auto&& first_args, auto&&... rest_args) noexcept {
    auto end = get<idx>() + size_each;
  
    std::apply(
      [end]<class... Args>(Args&&... args) {
        std::construct_at( end, std::forward<Args>(args)... );
      },
      std::forward<decltype(first_args)>(first_args)
    );
  
    if constexpr(sizeof...(Rest))
      add_to_end_unchecked_impl<idx + 1, Rest...>(size_each, std::forward<decltype(rest_args)>(rest_args)...);
  }

  template <sz_t idx, class First, class... Rest>
  edenInlineCXPR void 
  value_initialize_elements_impl(sz_t count) noexcept {
    auto begin = get<idx>();

    if constexpr(std::is_trivially_default_constructible_v<First>)
      std::memset(begin, 0, count * sizeof(First));
    else
      for(auto i{0uz}; i<count; ++i) std::construct_at(begin);

    if constexpr(sizeof...(Rest))
      return value_initialize_elements_impl<idx + 1, Rest...>(count);
  }
  
public:
  
  template<sz_t idx> edenInlineCXPR auto& get() noexcept { return std::get<idx>(ptrs); }
  template<sz_t idx> edenInlineCXPR auto& get() const noexcept { return std::get<idx>(ptrs); }
  edenInlineNodiscardCXPR auto buffer_begin_slice() noexcept { return std::get<buffer_begin_idx>(ptrs); }
  
  edenInlineCXPR void add_to_end_unchecked(sz_t size_each, auto&& first_args, auto&&... rest_args) noexcept { return add_to_end_unchecked_impl<0, Ts...>(size_each, std::forward<decltype(first_args)>(first_args), std::forward<decltype(rest_args)>(rest_args)...); }
  edenInlineCXPR void destroy_all(sz_t size_each) noexcept { return destroy_all_impl<0, Ts...>(size_each); }

  // discards current slices and sets them to alloc, sliced
  edenInlineCXPR void reslice_with(void* alloc, sz_t new_capacity_each) noexcept { return reslice_with_impl<0, Ts...>((byte_t*)alloc, new_capacity_each); }

  // relocates everything to new_slices
  edenInlineCXPR void relocate_to(Slices new_slices, sz_t size_each) noexcept { return relocate_impl<0, Ts...>(new_slices, size_each); }
  
  edenInlineCXPR void value_initialize_elements(sz_t count) noexcept { return value_initialize_elements_impl<0, Ts...>(count); }
};

}

// TODO: 
// - Add settings and custom allocator support
// - Explore potential size optimization where we only store one pointer and two sz_ts, slicing is done on-demand instead. Probably not very performant but it may matter.
template <class... Ts>
requires (sizeof...(Ts) > 1)
class contiguous_soa {
  static constexpr auto NumTs = sizeof...(Ts);
  static constexpr auto ExpansionMult = 2;
  static constexpr packs::PackData<Ts...> pack_data{};
  static constexpr auto FirstAllocCapacity = 1;

  detail::Slices<Ts...> slices;
  sz_t size_each{};
  sz_t capacity_each{};

  static constexpr bool doAlignedAllocation = pack_data.biggest_alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__;
  
  edenInlineNodiscard static void* allocate(sz_t num_bytes) noexcept {
    if constexpr(doAlignedAllocation)
      return (byte_t*)::operator new(num_bytes, (align_t) pack_data.biggest_alignment); 
    else 
      return (byte_t*)::operator new(num_bytes); 
  }
  edenInlineCXPR static void deallocate_at(void* alloc, sz_t alloc_size_bytes) noexcept { 
    if constexpr(doAlignedAllocation)
      ::operator delete(alloc, alloc_size_bytes, (align_t) pack_data.biggest_alignment);
    else 
      ::operator delete(alloc, alloc_size_bytes);
  }
  edenInlineCXPR void deallocate() noexcept { deallocate_at(slices.buffer_begin_slice(), buffer_size_bytes()); }

  edenInlineNodiscardCXPR sz_t buffer_size_bytes() const noexcept { return capacity_each * pack_data.total_size; }

  edenInlineCXPR void 
  destroy_all() noexcept { 
    slices.destroy_all(size_each);
    size_each = 0; 
  }

  void expand_to(sz_t new_capacity_each) noexcept {
    assert(new_capacity_each >= capacity_each);
    auto const old_buffer_size_bytes = buffer_size_bytes();
    capacity_each = new_capacity_each;
    auto const new_alloc = allocate(buffer_size_bytes());
    auto const old_alloc = slices.buffer_begin_slice();
    
    auto old_slices = slices;
    slices.reslice_with(new_alloc, new_capacity_each);

    // this branch isn't strictly necessary
    // TODO: benchmark with and without
    if(size_each != 0) old_slices.relocate_to(slices, size_each);
    deallocate_at(old_alloc, old_buffer_size_bytes);
  }

  void allocate_from_empty(sz_t new_capacity_each = FirstAllocCapacity) noexcept {
    assert(slices.buffer_begin_slice() == nullptr); assert(size_each == 0); assert(capacity_each == 0);
    capacity_each = new_capacity_each;
    auto const new_allocation = allocate( buffer_size_bytes() );
    slices.reslice_with(new_allocation, new_capacity_each);
  }

  template <class ...ArgTuples>
  void grow_and_emplace(ArgTuples&&... args_for_each_element) noexcept {
    if (slices.buffer_begin_slice() == nullptr) {
      allocate_from_empty();
      slices.add_to_end_unchecked(size_each, std::forward<ArgTuples>(args_for_each_element)...);
      ++size_each;
      return;
    }
    assert(size_each != 0);

    auto const new_capacity_each = capacity_each * ExpansionMult; assert(new_capacity_each >= capacity_each);
    auto const old_buffer_size_bytes = buffer_size_bytes();
    capacity_each = new_capacity_each;
    auto const new_alloc = allocate(buffer_size_bytes());
    auto const old_alloc = slices.buffer_begin_slice();
    
    auto old_slices = slices;
    slices.reslice_with(new_alloc, new_capacity_each);
    slices.add_to_end_unchecked(size_each, std::forward<ArgTuples>(args_for_each_element)...);
    
    old_slices.relocate_to(slices, size_each);
    deallocate_at(old_alloc, old_buffer_size_bytes);
    ++size_each;
  }

public:

  constexpr contiguous_soa() = default;

  explicit contiguous_soa(sz_t count) noexcept {
    allocate_from_empty(count);
    slices.value_initialize_elements(count);
    size_each = count;
  }

  template <sz_t N>
  explicit contiguous_soa(flags::ReserveInitial<N>) noexcept
  { allocate_from_empty(N); }

  constexpr contiguous_soa(contiguous_soa&& other) noexcept
  : slices(other.slices), size_each(other.size_each), capacity_each(other.capacity_each) {
    other.slices = {};
    other.size_each = 0; other.capacity_each = 0;
  }

  contiguous_soa& operator=(contiguous_soa&& other) noexcept {
    destroy_all(); deallocate();
    slices = other.slices;
    size_each = other.size_each; 
    capacity_each = other.capacity_each;

    other.slices = {};
    other.size_each = 0; 
    other.capacity_each = 0;
    return *this;
  }

  constexpr ~contiguous_soa() {
    destroy_all();
    deallocate();
  }

  edenInlineNodiscardCXPR sz_t total_size()          const noexcept { return size_each * NumTs; }
  edenInlineNodiscardCXPR sz_t individual_size()     const noexcept { return size_each; }
  edenInlineNodiscardCXPR sz_t total_capacity()      const noexcept { return capacity_each * NumTs; }
  edenInlineNodiscardCXPR sz_t individual_capacity() const noexcept { return capacity_each; }
  edenInlineNodiscardCXPR bool empty()               const noexcept { return size_each == 0; }
  edenInlineCXPR          void clear()                     noexcept { destroy_all(); }

  template <sz_t IDX> edenInlineNodiscardCXPR auto&       front()        noexcept { assert(size_each not_eq 0); return slices.template get<IDX>()[0]; }
  template <sz_t IDX> edenInlineNodiscardCXPR auto const& front()  const noexcept { assert(size_each not_eq 0); return slices.template get<IDX>()[0]; }
  template <sz_t IDX> edenInlineNodiscardCXPR auto&       back()         noexcept { assert(size_each not_eq 0); return slices.template get<IDX>()[size_each - 1]; }
  template <sz_t IDX> edenInlineNodiscardCXPR auto const& back()   const noexcept { assert(size_each not_eq 0); return slices.template get<IDX>()[size_each - 1]; }
  template <sz_t IDX> edenInlineNodiscardCXPR auto*       data()         noexcept { return slices.template get<IDX>(); }
  template <sz_t IDX> edenInlineNodiscardCXPR auto const* data()   const noexcept { return slices.template get<IDX>(); }

  template <sz_t IDX> edenInlineNodiscardCXPR auto to_span()       noexcept { return std::span( data<IDX>(), size_each ); }
  template <sz_t IDX> edenInlineNodiscardCXPR auto to_span() const noexcept { return std::span( data<IDX>(), size_each ); }

  void reserve(sz_t new_capacity_each) noexcept {
    if(capacity_each >= new_capacity_each) return;
    if (slices.template get<0>() != nullptr) // shouldn't matter which slice we check
      expand_to(new_capacity_each);
    else
      allocate_from_empty(new_capacity_each);     
  }

  template <class... ArgTuples> // should be tuples of arguments, one tuple per member
  requires (sizeof...(ArgTuples) == NumTs)
  void emplace_back(ArgTuples&&... args_for_each_element) noexcept {
    if(size_each != capacity_each) {
      slices.add_to_end_unchecked(size_each, std::forward<ArgTuples>(args_for_each_element)...);
      ++size_each;
      return;
    }
    
    grow_and_emplace(std::forward<ArgTuples>(args_for_each_element)...);
  }

  template <class... ArgTuples> // should be tuples of arguments, one tuple per member
  requires (sizeof...(ArgTuples) == NumTs)
  void emplace_back_unchecked(ArgTuples&&... args_for_each_element) noexcept {
    assert(size_each != capacity_each);
    slices.add_to_end_unchecked(size_each, std::forward<ArgTuples>(args_for_each_element)...);
    ++size_each;
  }

  template <class... Args>
  edenAlwaysInline void push_back(Args&&... new_elements) noexcept { 
    return emplace_back(
      std::forward_as_tuple(std::forward<Args>(new_elements))...
    );
  }

  template <class... Args>
  edenAlwaysInline void push_back_unchecked(Args&&... new_elements) noexcept { 
    return emplace_back_unchecked(
      std::forward_as_tuple(std::forward<Args>(new_elements))...
    ); 
  }

  template<sz_t IDX> 
  edenInlineCXPR auto&
  get(sz_t idx) noexcept {
    assert(idx < size_each);
    return data<IDX>()[idx];
  }

};

}
