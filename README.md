### Made in C++23

This is a collection of containers, functions, and utilities that i've created for one of many reasons:
- The standard library doesn't provide a way of doing what I wanted (releasing_vector)
- The standard library has a limited or annoying implementation of what I wanted (owned_ptr)
- Convenience (assume_assert, typedefs, lifetime_observer, enum_utils, etc)

---
Here is a quick summary of the smaller headers:
- arena.hpp: arena allocator with a link to another arena if capacity fills.
- assume_assert.hpp: shorthand for an assertion followed by an [[assume]] attribute.
- null_conditional_chaining.hpp: macro and template based implementation of a null conditional operator.
  - ```cpp
    First* first = getfirst(arg1);
    Second* second = first->getsecond(arg2);
    if(second) {
        Third* third = second->getthird(arg3);
        if(third) {
            third->doThing();
        }
    } 
    //can be written as below with the same resulting assembly 
    call_methods_conditionally(getfirst(arg1),
                               next_method(First::getsecond, arg2),
                               next_method(Second::getthird, arg3),
                               next_method(Third::doThing) );
    //methods must all return pointers, except last which may return void
    ```
- lifetime_observer.hpp
  - Logs all constructions, assignments, and destructions.
  - Can create observer groups by providing group number in template.
  - Logs are formatted with each instance's ID and group number.
- type_flags.hpp
  - Flags that can be used across my library for various purposes.
- typedefs.hpp
  - Shortened type names for common types.
---
### Owned Pointer
A version of unique_ptr without deletion. <br>
Serves primarily to communicate the intent of ownership, without some of the drawbacks of unique_ptr.
- unique_ptr sometimes requires types to be complete even when they really don't need to be, making idioms like PIMPL harder.
- Cannot be safely used with non-dynamically allocated values.

Additional utilities are provided for array types.
- Conversion to static span for bounded array types.
- operator==(const owned_ptr&) performs an element-wise comparison if available and bounded.

c_str specialization provided, with more utilities:
- operator std::string_view
- operator std::string
- length()

### Owned Span
Similar to owned_ptr, but for span.

### Releasing Vector
An implementation of a vector able to 'release' ownership over its internal buffer. <br>
If the data is released, but needs to be accessed as if it were a vector again, a new releasing_vector can claim ownership. <br>
When the data is no longer needed, the pointer can be passed to a static method which will handle the destruction and deallocation. <br>
Iterator invalidation rules are consistent with std::vector. 
- After release iterators are guaranteed remain valid, provided the data is not reclaimed by another vector.

'Settings' are provided allowing you to specify:
- Expansion multiplier.
- Whether the vector allocates a header alongside the data or provides the user with a handle.
- Whether to enable string capabilities (will always release a null terminated array).

Note that no exception safety is guaranteed, because exceptions are lame and I don't care.
- As such, the noexcept status of any given method is determined by whether the allocator and/or value type is noexcept.

Essentially all methods are constexpr.

API:
```cpp
//constexpr and noexcept will be excluded for brevity.
//methods that are identical to their std::vector counterparts will be excluded too.
    
/*  Constructors / Assignment */

    template <sz_t N> 
    explicit (flags::ReserveInitial<N>); //pass in a flags::reserve_initial<N> instance 
    
    explicit (released_ptr released_data) requires store_header;    //reclaims ownership over data previously released
    explicit (released_span released_data) requires store_header;   //same as above
     
    //copy constructor / assignment not implemented at the moment.
 
    //move constructor / assignment / swap defined only for releasing vectors with compatable settings
    //settings are compatable if their StoreHeader value is the same 
/*  Constructors / Assignment */

/*  Release and Deletion */

    //releases the data in the form of a released_ptr / released_span (typedef for owned_ptr<T[]> / owned_span<T>)
    released_ptr release() requires store_header; 
    released_span release_span() requires store_header; 
    
    //releases a handle for the data containing the allocator, owned_ptr<T[]>, size, and capacity
    //the handle's destructor will destroy and deallocate the data properly
    data_handle release() requires (not store_header);
    
    //destroys and deallocates the released data when it is no longer needed
    static void destroy_and_dellocate(released_ptr data) requires store_header; 
    
    //returns a released_ptr to element-wise copied array, including a new header
    //equivalent to constructing new releasing_vector, reserving the exact size, copying all elements into it, then releasing
    static released_ptr copy_data(const released_ptr& data) requires store_header and copy_constructible<T>;
    
    //returns the size or capacity of the data
    static sz_t data_size(const released_ptr& data) require store_header;
    static sz_t data_capacity(const released_ptr& data) require store_header;
/*  Release and Deletion */

/*  Additional Utilities */

    explicit operator std::span<T>() const;
    std::span<T> to_span() const;
    
    /* CString specific utilities */ 
    template <sz_t N>
    explicit (const char(&c_str)[N]) requires is_string; //string literal constructor
    
    operator std::string_view() const;
    std::string_view to_stringview() const;
    explicit operator std::string() const;
    std::string to_stdstring() const;
    
    template<sz_t N>
    bool operator==(const char(&c_str)[N]);
    
    template<sz_t N>
    bool operator==(const std::string& std_str);  
    /* CString specific utilities */

/*  Additional Utilities */
   
```

### Concepts
Introduces the 'type' class, allowing for more convenient template meta-programming. <br>
```cpp
/* Shortened names for common concepts and traits */
    std::remove_reference_t<T>
    type<T>::no_ref //equivalent
    
/* Reads naturally from left to right */
    std::is_same_v<T, U>    std::is_constructible_v<T, Args...>;
    type<T>::is<U>          type<T>::constructible_with<Args...>;
     
/* Common / Useful transforms and properties */
    type<int>::opposite_sign; //unsigned
    type<int[2][3]>::num_dimensions; //2
    type<int[2][8][5]>::dimension_size<1>; //8
    type<int>::is_one_of<int, unsigned, char>; // true
    type<int>::is_none_of<int, char>; // false
    type<const int&>::no_cvref; //int

/* Can be inherited from, allowing for cleaner syntax */
    struct Example : type<Example> {};     
    using example_array = Example::as_array;
    
    //registered_type concept can be used to filter for parameters inheriting from type
    template <registered_type T>
    auto to_array(T&& obj) 
    {...}
 
/* Type chaining can be achieved by appending a '_' on every type but the last */
    Example::as_const_::as_array_::as_ptr x; //const Example(*)[]
    type<int>::as_bounded_array_<5>::as_bounded_array<6> y; //int[6][5]
    
/* Types may be represented as variables */
    //type_variable concept filters for type<...> instances
    consteval bool arbitrary_condition(type_variable auto var) {    
        //_i suffix may be applied to get a transformed type variable
        return var.is_void || 
               var.no_ptr_i().is_void || 
               other_condition(var) ||
               var == type<int>{};
    } 
    static constexpr bool meets_condition = arbitrary_condition(Example::type_instance());

/* A type can 'smuggle' itself as another type */
    struct Nefarious : type<unsigned> {}; //does not satisfy registered_type concept
    Nefarious::is_integral //true
    Nefarious::as_signed //int
    type<Nefarious>::is_integral //false
 
/* Types can be given string representations */
    struct BootlegReflection : type<BootlegReflection, "This Language Sucks!"> {};
    std::cout << BootlegReflection::name;
    
    //append_number_to_literal helper provided
    template <sz_t N>
    struct Numbered : type<Numbered<N>, append_number_to_literal<N, "Numbered">> {};
    Numbered<3> x;
    std::cout << x.name; //Numbered<3>
```