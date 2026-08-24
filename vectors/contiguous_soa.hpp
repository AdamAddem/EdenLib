#pragma once
#include "../metaprogramming/packs.hpp"
#include "../type_flags.hpp"
#include "../typedefs.hpp"

#include <cstring>
#include <format>
#include <utility>
#include <span>
#include <tuple>
#include <stdexcept>
#include <new>
#include <type_traits>
#include <cassert>

namespace eden {

#define launder_cast(Type, ptr) std::launder( (Type*) (ptr) )
#define launder_castT(ptr) std::launder( (T*) (ptr) )

namespace detail {

template <class First, class... Rest>
eden_always_inline static constexpr void
destroy_all(byte_t** begins, sz_t size_each) noexcept {
  if(size_each == 0) return;
  if constexpr(sizeof...(Rest)) destroy_all<Rest...>(begins + 1, size_each);

  --size_each;
  if constexpr(not std::is_trivially_destructible_v<First>) {
    auto const first_begin = begins[0];
    byte_t* end = first_begin + size_each * sizeof(First);
    while(true) {
      std::destroy_at( launder_cast(First, end) );
      if(end == first_begin) break;
      end -= sizeof(First);
    }
  }
}

template <class First, class... Rest>
eden_always_inline static constexpr void
relocate_slices(byte_t** old_slices, byte_t** new_slices, sz_t size_each) noexcept {
  if(size_each not_eq 0) {
    First* old_slice = launder_cast(First, old_slices[0]);
    First* construct_location = (First*) new_slices[0];
  
    if constexpr(eden_trivially_relocatable(First))
      std::memcpy( construct_location, old_slice, size_each * sizeof(First) );
    else
      for(auto i{0uz}; i<size_each; ++i)
        std::construct_at( construct_location + i, std::move(old_slice[i]) ), 
        std::destroy_at(old_slice + i); // technically not reverse order but who really cares
  }

  if constexpr(sizeof...(Rest)) {
    relocate_slices<Rest...>(old_slices + 1, new_slices + 1, size_each);
  }
}

template <class First, class... Rest>
eden_always_inline static constexpr void
add_to_end_unchecked(byte_t** begins, sz_t size_each, auto&& first_args, auto&&... rest_args) noexcept {
  First* open_slot = ( (First*) begins[0] ) + size_each;

  std::apply(
    [open_slot]<class... Args>(Args&&... args){
      std::construct_at(
        open_slot,
        std::forward<Args>(args)...
      );
    },
    std::forward<decltype(first_args)>(first_args)
  );

  if constexpr(sizeof...(Rest))
    add_to_end_unchecked<Rest...>(begins + 1, size_each, std::forward<decltype(rest_args)>(rest_args)...);
}

template <class First, class... Rest>
eden_always_inline static constexpr void
start_slice_lifetimes(byte_t** begins, sz_t new_capacity_each) noexcept {
  begins[0] = (byte_t*) std::start_lifetime_as_array<First>(begins[0], new_capacity_each);

  if constexpr(sizeof...(Rest))
    start_slice_lifetimes<Rest...>(begins + 1, new_capacity_each);
}

}

// TODO: 
// - Add settings and custom allocator support
template <class... Ts>
requires (sizeof...(Ts) > 1)
class contiguous_soa {
  static constexpr auto NumTs = sizeof...(Ts);
  static constexpr auto ExpansionMult = 2;
  static constexpr packs::SizePack<Ts...> Sizes{};
  static constexpr packs::AlignPack<Ts...> Alignments{};
  static constexpr auto FirstAllocCapacity = 1;
  static constexpr auto buffer_begin_idx = Alignments.alignidx_to_typeidx[0];

  eden_always_inline [[nodiscard]] static constexpr sz_t alignidx_to_typeidx(sz_t type_idx) { return Alignments.alignidx_to_typeidx[type_idx]; }

  template <sz_t IDX>
  using type_at_idx = packs::type_at_idx<IDX, Ts...>;

  byte_t* slices[NumTs]{}; // slices in Ts order, the beginning of the buffer is given by slices[buffer_begin_idx]
  sz_t    size_each{};
  sz_t    capacity_each{};

  eden_always_inline [[nodiscard]] static byte_t* allocate(sz_t num_bytes) noexcept { return (byte_t*)::operator new(num_bytes, (align_t) Alignments.biggest_alignment); }
  eden_always_inline static constexpr void deallocate_at(byte_t* alloc, sz_t alloc_size_bytes) noexcept { ::operator delete(alloc, alloc_size_bytes, (align_t)Alignments.biggest_alignment); }
  eden_always_inline constexpr void deallocate() noexcept { deallocate_at(slices[buffer_begin_idx], buffer_size_bytes()); }

  eden_always_inline [[nodiscard]] constexpr sz_t buffer_size_bytes() const noexcept { return capacity_each * Sizes.total_size; }

  // overrides begins with new_allocation sliced properly
  eden_always_inline constexpr void 
  initialize_slices(byte_t* new_allocation, sz_t new_capacity_each) {
    slices[ buffer_begin_idx ] = new_allocation;
    for(auto i{1uz}; i<NumTs; ++i)
      slices[alignidx_to_typeidx(i)] = slices[alignidx_to_typeidx(i-1)] + new_capacity_each * Sizes[alignidx_to_typeidx(i-1)];

    detail::start_slice_lifetimes<Ts...>(slices, new_capacity_each);
  }

  eden_always_inline static constexpr void
  destroy_all_at(byte_t** slices, sz_t size_each) noexcept {
    detail::destroy_all<Ts...>(slices, size_each);
  }

  eden_always_inline constexpr void destroy_all() noexcept { destroy_all_at(slices, size_each); size_each = 0; }

  void expand_to(sz_t new_capacity_each) noexcept {
    assert(new_capacity_each >= capacity_each);
    auto const old_buffer_size_bytes = buffer_size_bytes();
    capacity_each = new_capacity_each;
    auto const new_alloc = allocate(buffer_size_bytes());
    auto const old_alloc = slices[buffer_begin_idx];
    
    decltype(slices) old_slices; std::memcpy(old_slices, slices, sizeof(slices));
    initialize_slices(new_alloc, new_capacity_each);
    
    detail::relocate_slices<Ts...>(old_slices, slices, size_each);
    deallocate_at(old_alloc, old_buffer_size_bytes);
  }

  void allocate_from_empty(sz_t new_capacity_each = FirstAllocCapacity) noexcept {
    assert(slices[0] == nullptr); assert(size_each == 0); assert(capacity_each == 0);
    capacity_each = new_capacity_each;
    auto const new_allocation = allocate( buffer_size_bytes() );
    initialize_slices(new_allocation, new_capacity_each);
  }

  template <class ...ArgTuples>
  constexpr void
  grow_and_emplace(ArgTuples&&... args_for_each_element) noexcept {
    if (slices[0] == nullptr) {
      allocate_from_empty();
      detail::add_to_end_unchecked<Ts...>(slices, size_each, std::forward<ArgTuples>(args_for_each_element)...);
      ++size_each;
      return;
    }

    auto const new_capacity_each = capacity_each * ExpansionMult; assert(new_capacity_each >= capacity_each);
    auto const old_buffer_size_bytes = buffer_size_bytes();
    capacity_each = new_capacity_each;
    auto const new_alloc = allocate(buffer_size_bytes());
    auto const old_alloc = slices[buffer_begin_idx];
    
    decltype(slices) old_slices; std::memcpy(old_slices, slices, sizeof(slices));
    initialize_slices(new_alloc, new_capacity_each);
    detail::add_to_end_unchecked<Ts...>(slices, size_each, std::forward<ArgTuples>(args_for_each_element)...);
    detail::relocate_slices<Ts...>(old_slices, slices, size_each);
    deallocate_at(old_alloc, old_buffer_size_bytes);
    ++size_each;
  }

  
public:

  constexpr contiguous_soa() = default;

  template <sz_t N>
  explicit contiguous_soa(flags::ReserveInitial<N>) noexcept
  { allocate_from_empty(N); }

  constexpr contiguous_soa(contiguous_soa&& other) noexcept
  : size_each(other.size_each), capacity_each(other.capacity_each) {
    for (auto i{0uz}; i<NumTs; ++i) {
      slices[i] = other.slices[i];
      other.slices[i] = nullptr;
    }
    other.size_each = 0; other.capacity_each = 0;
  }

  contiguous_soa&
  operator=(contiguous_soa&& other) noexcept  {
    destroy_all(); deallocate();
    size_each = other.size_each; capacity_each = other.capacity_each;
    for (auto i{0uz}; i<NumTs; ++i) {
      slices[i] = other.slices[i];
      other.slices[i] = nullptr;
    }

    other.size_each = 0; other.capacity_each = 0;
    return *this;
  }

  constexpr ~contiguous_soa() {
    destroy_all();
    deallocate();
  }

  eden_always_inline [[nodiscard]] constexpr sz_t total_size()          const noexcept { return size_each * NumTs; }
  eden_always_inline [[nodiscard]] constexpr sz_t individual_size()     const noexcept { return size_each; }
  eden_always_inline [[nodiscard]] constexpr sz_t total_capacity()      const noexcept { return capacity_each * NumTs; }
  eden_always_inline [[nodiscard]] constexpr sz_t individual_capacity() const noexcept { return capacity_each; }
  eden_always_inline [[nodiscard]] constexpr bool empty()               const noexcept { return size_each == 0; }
  eden_always_inline               constexpr void clear()                     noexcept { destroy_all(); }

  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto&       front()        noexcept { assert(size_each not_eq 0); return reinterpret_cast<type_at_idx<IDX>*>(slices[IDX])[0]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto const& front()  const noexcept { assert(size_each not_eq 0); return reinterpret_cast<type_at_idx<IDX>*>(slices[IDX])[0]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto&       back()         noexcept { assert(size_each not_eq 0); return reinterpret_cast<type_at_idx<IDX>*>(slices[IDX])[size_each-1]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto const& back()   const noexcept { assert(size_each not_eq 0); return reinterpret_cast<type_at_idx<IDX>*>(slices[IDX])[size_each-1]; }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto*       data()         noexcept { return reinterpret_cast<type_at_idx<IDX>*>(slices[IDX]); }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr auto const* data()   const noexcept { return reinterpret_cast<type_at_idx<IDX>*>(slices[IDX]); }

  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr std::span< type_at_idx<IDX> >      to_span()                 noexcept { return std::span( reinterpret_cast<type_at_idx<IDX>*>(slices[IDX]), size_each ); }
  template <sz_t IDX> eden_always_inline [[nodiscard]] constexpr std::span< type_at_idx<IDX> const> to_span()           const noexcept { return std::span( reinterpret_cast<type_at_idx<IDX>*>(slices[IDX]), size_each ); }

  void reserve(sz_t new_capacity_each) noexcept {
    if(capacity_each >= new_capacity_each) return;
    if (slices[0] not_eq nullptr) // shouldn't matter which slice we check
      expand_to(new_capacity_each);
    else
      allocate_from_empty(new_capacity_each);     
  }

  template <class... ArgTuples> // should be tuples of arguments, one tuple per member
  requires (sizeof...(ArgTuples) == NumTs)
  void emplace_back(ArgTuples&&... args_for_each_element) noexcept {
    if(size_each == capacity_each) [[unlikely]]
      return grow_and_emplace(std::forward<ArgTuples>(args_for_each_element)...);
    
    detail::add_to_end_unchecked<Ts...>(slices, size_each, std::forward<ArgTuples>(args_for_each_element)...);
    ++size_each;
  }

  template <class... Args>
  eden_always_inline void push_back(Args&&... new_elements) noexcept { 
    return emplace_back(
      std::forward_as_tuple(std::forward<Args>(new_elements))...
    ); 
  }

  template<sz_t IDX> 
  eden_always_inline constexpr auto&
  get(sz_t idx) noexcept {
    assert(idx < size_each);
    return launder_cast( type_at_idx<IDX>, slices[IDX] )[idx];
  }

  template<sz_t IDX> constexpr auto&
  get_at(sz_t idx) eden_throws(std::out_of_range) {
    if (idx < size_each)
      return launder_cast( type_at_idx<IDX>, slices[IDX] )[idx];
    throw std::out_of_range( std::format("Element access at index {} in eden::contiguous_soa with individual size of {}.", idx, individual_size()) );
  }

};
}
#undef launder_cast
#undef launder_castT