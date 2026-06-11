### Note:
The vector implementations within this library do not have any exception safety guarantees. <br>
As such, each method is marked noexcept conditional on the exception behavior of the value_type / allocator in that scenario. <br>
Expansion still relies on std::move_if_noexcept. <br>
Each implementation has the same API as std::vector, with some additions listed below: 
```cpp
    template <sz_t N> 
    constexpr explicit (flags::ReserveInitial<N>)  // pass in a flags::reserve_initial<N> instance 
    noexcept(nothrow_allocating); 
    
    constexpr explicit operator std::span<T>() noexcept;
    constexpr std::span<T> to_span() noexcept;
    constexpr explicit operator std::span<const T>() const noexcept;
    constexpr std::span<const T> to_span() const noexcept;
```


### Vector 'Settings'
The vector implementations within this library each have custom settings, allowing you to specify some generic properties (expansion multiplier), 
and some implementation properties (stability in swap_vector). <br>
To use customize each vector, pass the settings object into the respective vector's template argument as a non-type parameter. <br>
I'd recommend that you create a type alias as writing the entire typename is tedious. <br>
```using my_customization = releasing_vector<char, releasing_vector_settings<true, 3>{}>; ```
The common settings between most vectors is 'Small' and 'ExpansionMult':
- Small will have the vector use a T* and two u32s for size and capacity, shrinking the vector's size to 16 bytes.
- ExpansionMult will specifies the expansion rate of the vector after reaching capacity.

### releasing_vector.hpp
An implementation of a vector able to 'release' ownership over its internal buffer. <br>
If the data is released, but needs to be accessed as if it were a vector again, a new releasing_vector can claim ownership. <br>
When the data is no longer needed, the pointer can handle its own destruction through a method. <br>
Iterator invalidation rules are consistent with std::vector.
- After release iterators are guaranteed to remain valid, provided the data is not reclaimed by another vector and subsequently changed.

Settings:
- Expansion multiplier.
- CString: Whether to enable string capabilities (will always release a null terminated array). Does not perform small-string-optimization.

API:
```cpp
// constexpr and noexcept will be excluded for brevity.
// methods identical to their std::vector counterparts are also excluded.
    
/*  Constructors / Assignment */

    template <sz_t N> 
    explicit (flags::ReserveInitial<N>); // pass in a flags::reserve_initial<N> instance 
    
    explicit (released_ptr released_data) requires store_header;    // reclaims ownership over data previously released
    explicit (released_span released_data) requires store_header;   // same as above
     
    // copy constructor / assignment not implemented at the moment.
 
    // move constructor / assignment / swap defined only for releasing vectors with compatable settings
    // settings are compatable if their CString value is the same 
    
/*  Constructors / Assignment */

/*  Release and Deletion */

    // releases the data in the form of a released_ptr / released_span
    released_ptr release(); 
    released_span release_span(); 
    
    // destroys and deallocates the released data when it is no longer needed
    static void destroy_and_dellocate(released_ptr data); 
    
    // returns a released_ptr to element-wise copied array
    // equivalent to constructing a new releasing_vector, reserving the exact size, copying all elements into it, then releasing
    static released_ptr copy_data(const released_ptr& data) requires copy_constructible<T>;
    
    // returns the size or capacity of the data
    static sz_t data_size(const released_ptr& data);
    static sz_t data_capacity(const released_ptr& data);
    
/*  Release and Deletion */
    
/* CString specific utilities */ 
    template <sz_t N> explicit (const char(&c_str)[N]); // string literal constructor
    
    operator std::string_view() const;
    std::string_view to_stringview() const;
    explicit operator std::string() const;
    std::string to_stdstring() const;

    template<sz_t N> bool operator==(const std::string& std_str);  
    template<sz_t N> bool operator==(const char(&c_str)[N]);
    
/* CString specific utilities */
```

### swap_vector.hpp
An implementation of vector with customizeable transposition / move-to-front linear search functionality. <br>
Elements are searched using a user provided predicate. When found, they are moved towards the back for faster searches in the future. <br>

Settings:
- Stability: The rate at which elements are moved towards the back (lower is faster). A setting of 1 will immediately swap elements with the backmost position.
- Small.
- Expansion multiplier.

API:
```cpp
  // returns nullptr if not found. pointer is unstable and may point to bad data should this vector expand or another search be called.
  template<class KeyType>
  [[nodiscard]] constexpr T*
  search(KeyType&& key)
  noexcept(nothrow_swappable)
  requires (convertible_to_c<std::invoke_result_t<decltype(BinaryPredicate), T const&, KeyType>, bool>); // predicate must accept T & key, and return something convertible to bool
  
 ```

Example Usage:
```cpp
  struct Foo { int x; };
  using my_swap_vec = swap_vector<Foo, [] (Foo const& element, int key) { return element.x == key; }>;
  my_swap_vec vec;
  ...
  auto* res = vec.search(5);
```


### Creating your own implementation 
base_vector.hpp holds a basic vector implementation that is (mostly) standards compliant with the main caveat of no exception safety. <br>
There exists a base_vector_settings class with two settings, Small and ExpansionMult (their effects are detailed in the header). <br>
To create your own implementation, use the following format: <br>
```cpp
template<u64_t custom_setting = 4, bool Small = false, u64_t ExpansionMult = 2>
struct custom_vector_settings {
  static constexpr auto my_setting = custom_setting;
  static constexpr eden::base_vector_settings<Small, ExpansionMult> base_settings{};
};

template <class T, auto settings = custom_vector_settings{}, eden::allocator_for_c<T> Allocator = std::allocator<T>>
class custom_vector : public eden::base_vector<T, custom_vector<T, settings, Allocator>, settings.base_settings, Allocator> {
  static constexpr auto my_setting = settings.my_setting;
  
  using base = eden::base_vector<T, custom_vector, settings.base_settings, Allocator>; // Recommended for easy use of base_vector's members.
  friend class eden::base_vector<T, custom_vector, settings.base_settings, Allocator>; // Required if you override any of the protected members of base_vector.
  using base::m_begin; // Brings base class members into scope. Required if you don't want to use base:: prefix (due to templating quirks).
public:
};
```
Any overriden methods in the derived class will automatically be preferred anywhere they are used in base_vector, even when called directly on a base_vector&. <br>
Note that if you plan to use the m_size and m_cap members directly, you must accommodate for the possibility that your vector is 'Small'.
Normally they are of type T*, but when 'Small' is true they will be of type u32_t. 
If you don't plan on manipulating these members, prefer to use the size() and capacity() methods instead. <br>
To prevent use of a setting in base_vector don't include it as a template paremeter in your settings class. Provide your preferred value directly to base_vector_settings. <br>
