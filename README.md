### Made in C++23

This is a collection of containers, functions, and utilities that i've created for one of many reasons:
- The standard library doesn't provide a way of doing what I wanted
  - releasing_vector
- The standard library has a limited or annoying implementation of what I wanted
  - owned_ptr
- Convenience
  - assume_assert, typedefs, lifetime_observer, enum_utils

---
Here is a quick summary of the smaller headers:
- arena.hpp
  - arena allocator with a link to another arena if capacity fills.
- assume_assert.hpp
  - shorthand for an assertion followed by an [[assume]] attribute | Ex: assert(condition); [[assume(condition)]];
- null_conditional_chaining.hpp
  - macro and template based implementation of a null conditional operator.
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
      First* first = getfirst(arg1);
      call_methods_conditionally(first,
                                 next_method(First::getsecond, arg2),
                                 next_method(Second::getthird, arg3),
                                 next_method(Third::doThing) );
      ```
- lifetime_observer.hpp
  - Each lifetime observer has its own ID, and it will log all constructions, assignments, and destructions.
  - Can create different groups using template.
- type_flags.hpp
  - Flags that can be used across my library for various purposes.
- typedefs.hpp
  - Shortened type names for common types.
---
### Releasing Vector
An implementation of a vector able to 'release' ownership over its internal buffer. <br>
If the data is released, but needs to be accessed as if it were a vector again, a new releasing_vector can claim ownership. <br>
When the data is no longer needed, the pointer can be passed to a static method which will handle the destruction and deallocation. <br>

'Settings' are provided allowing you to specify:
- Initial allocation size.
- Expansion multiplier.
- Whether the vector allocates a header alongside the data or provides the user with a handle.
- Whether to enable string capabilities (releases a null terminated array)

### Concepts
Introduces the 'type' class, allowing for more convenient template meta-programming. <br>
```cpp
-Shortened names for common concepts and traits. 
    std::remove_reference_t<T>
    type<T>::no_ref //equivalent
    
-Reads naturally from left to right.
    std::is_same_v<T, U>         std::is_constructible_v<T, Args...>;
    type<T>::same_as<U>          type<T>::constructible_with<Args...>;
     
-Niche helper types and properties.
    type<int>::opposite_sign //unsigned
    type<int[2][3]>::num_dimensions //2
    type<int[2][8][5]>::dimension_size<1> //8

-Can be inherited from, allowing for cleaner syntax and type chaining
    struct Example : type<Example> {};
    Example::as_const::no_volatile::as_bounded_array<5>

-A type can 'smuggle' itself as another type, only when accessed through the type itself
    struct Nefarious : type<unsigned> {};
    Nefarious::is_integral //true
    Nefarious::as_signed //int
    type<Nefarious>::is_integral //false
    
-Types can be given string representations
    struct BootlegReflection : type<BootlegReflection, "This Language Sucks!"> {};
    std::cout << BootlegReflection::name << std::endl;
```
### Owned Pointer
A version of unique_ptr without the deletion. <br>
Serves primarily to communicate the intent of ownership, without the drawbacks of unique_ptr.
- Specifically it sometimes requires types to be complete even when they really don't need to be. owned_ptr is agnostic in this regard.

Additional utilities are provided for array types.
- Conversion to static span for bounded array types.
- operator==(const owned_ptr&) performs an element-wise comparison if available and bounded.

c_str specialization provided, with more utilities:
- operator std::string_view
- operator std::string
- length()