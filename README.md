### Made in C++23

This is a collection of containers, functions, and utilities that i've created for one of many reasons:
- The standard library doesn't provide a way of doing what I wanted (vectors)
- The standard library has a limited or annoying implementation of what I wanted (owned)
- Convenience (assume_assert, typedefs, lifetime_observer, enum_utils)
- Eh, might as well (arena, macros, null_conditional_chaining, metaprogramming)

The README in the vectors directory provides some information on the vector implementations, 
when you'd want to use them, and how to leverage base_vector.hpp to easily create your own implementation. <br>

---
Here is a quick summary of the smaller headers:
- arena.hpp: arena + arenapool that stores a growing list of arenas
- macros.hpp: has macros that generalize some compiler specific features (__restrict, nonnull, etc). If they aren't supported, then the macro is empty.
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
- enum_utils.hpp
  - Provides enumBetween, a utility allowing for compile or runtime checking of the ordering of enums.
  - Whether the upper or lower bounds are inclusive or exclusive can be changed (default inclusive for both).
- string_utils.hpp
  - Provides the TemplateString class, a way of creating and manipulating strings at compile time.
  - Provides stpcpy, a portable version of POSIX's stpcpy.
---
###  owned.hpp
owned_ptr, A version of unique_ptr without deletion. <br>
Serves primarily to communicate the intent of ownership, without some of the drawbacks of unique_ptr.
- unique_ptr sometimes requires types to be complete even when they really don't need to be, making idioms like PIMPL harder.
- Cannot be easily used with non-dynamically allocated values.

Additional utilities are provided for array types.
- Conversion to static span for bounded array types.
- operator==(const owned_ptr&) performs an element-wise comparison if available and bounded.

c_str specialization provided, with more utilities:
- operator std::string_view
- operator std::string
- length()

owned_span is also included in this header, with identical semantics but for span rather than unique_ptr.

### metaprogramming/type_class.hpp
Introduces the 'type' class, allowing for more convenient template meta-programming and the representation of types as first-class citizens. <br>
Also features an implementation of type_list and a 'nontype_list'. <br>

#### type
```cpp
/* Shortened names for common concepts and traits */
    std::remove_reference_t<int&> -> type<int&>::no_ref
    
/* Reads naturally from left to right */
    std::is_same_v<int, float> -> type<int>::is<float>
    std::is_constructible_v<Box, int, float> -> type<Box>::constructible_with<int, float>;
     
/* Common and useful transforms / properties */
    type<int>::opposite_sign;                  // unsigned
    type<int[2][3]>::num_dimensions;           // 2
    type<int[2][8][5]>::dimension_size<1>;     // 8
    type<int>::is_one_of<int, unsigned, char>; // true
    type<int>::is_only_one_of<int, int, char>; // false
    type<int>::is_none_of<int, char>;          // false
    type<const int&>::no_cvref;                // int

/* Can be inherited from allowing for cleaner syntax w/ frequently reflected types */
    struct Example : type<Example> {};     
    using example_array = Example::as_array;    // Example[]
    
    // registered_type_c concept can be used to filter for parameters inheriting from type
    auto to_array(registered_type_c auto obj) 
    {...}
    
    struct Override : type<Override> {
        template<class T> static constexpr bool is = true;
    };
    static constexpr bool res = Override::is<float>; // true
 
/* Type chaining can be achieved by appending a '_' on every transform but the last */
    Example::as_const_::as_array_::as_ptr x;                // const Example(*)[]
    type<int>::as_bounded_array_<5>::as_bounded_array<6> y; // int[6][5]
    
/* Traits can be prepended with '_' to create a function which avoids the 'template' keyword and accepts instances */
    template<class T> concept high_cortisol   = type<T>::template is<Example>;
    template<class T> concept medium_cortisol = type<T>::_is(type_i<Example>);
    template<type T>  concept low_cortisol    = T._is(Example{});
    
/* Types may be instantiated as variables */
    consteval bool arbitrary_condition(registered_type_c auto var) {    
        // _i suffix may be applied to transforms to get a transformed type variable
        return var.is_void              || 
               var.no_ptr_i().is_void   || 
               other_condition(var)     ||
               var == type<int>{}       ||  // equality comparison is shorthand for std::is_same_v
               var == type_i<int>;          // type_i<...> is an alternative to type<...>{}
    } 
    static constexpr bool meets_condition = arbitrary_condition(type_i<Example>);
    static constexpr type deduced = 3;      // everything is implicitly convertible to type<...>
    unwrap<deduced>                         // int

/* A type may 'smuggle' itself as another type */
    struct Nefarious : type<unsigned> {};
    Nefarious::is_integral                // true
    Nefarious::as_signed                  // int
    type<Nefarious>::is_integral          // false
    registered_type_c<Nefarious>          // true
    properly_registered_type_c<Nefarious> // false
 
/* Types can be given string representations */
    struct BootlegReflection : type<BootlegReflection, "This Language Sucks!"> {};
    std::print("{}", BootlegReflection::name); // This Language Sucks!
    
    // append_number_to_literal helper provided
    template <sz_t N>
    struct Numbered : type<Numbered<N>, append_number_to_literal<N, "Numbered">> {};
    Numbered<3> x;
    std::cout << x.name; // Numbered<3>
```
#### type_list and nontype_list
```cpp
    /* type_list */
    // The following methods are all static but shown through the perspective of the variable.
    type_list<int, float> list; auto identical = type_i<int> + type_i<float>; auto identical2 = type_list{type_i<int>, type_i<float>};      
    auto first = list.current();                   // type<int>
    auto second = list.next().current();           // type<float>
    auto indexed = list.type_at<1>();              // type<float>
    std::is_same_v< float, decltype(list)::at<1> >;// true
    sz_t len = list.length;                        // 2
    
    auto add_char = list.append<char>(); decltype(add_char);                        // type_list<int, float, char>
    auto add_bool = list.append(type_i<bool>); decltype(add_bool);                  // type_list<int, float, bool>
    auto add_another = list.append(type_list<bool, char>{}); decltype(add_another); // type_list<int, float, bool, char>
    
    
    /* nontype_list */
    nontype_list<0, 1.0f, 'h'> list;
    auto first = list.current();            // 0, int
    auto second = list.next().current();    // 1, float
    auto third =  list.at<2>();             // 'h', char    
    auto third_t = list.type_at<2>();       // type<char>
    
    auto add_i = list.append(nontype_list<'i'>{});         // nontype_list<0, 1.0f, 'h', 'i'>
    auto add_ello = list.append<TemplateString{"ello"}>(); // nontype_list<0, 1.0f, 'h', TemplateString{"ello"}>
    
    /* type_list and nontype_list interop */
    nontype_list<104, 101.52, 'l', 108uz, 111z> data;
    auto typed_out = list.as_typelist();                   // type_list<int, double, char, std::size_t, std::ssize_t>
    type_list<int, int> int_types;
    type_list<char, char, char, char> char_types;
    
    // filters recieve nontype values and static cast to the respective type inside the type list
    auto filtered_vals = int_types.filtered_values<3.4f, 0.2>;  // nontype_list<3, 0>
    auto filtered_data = char_types.filter_nontypes(data);      // nontype_list<'h', 'e', 'l', 'l', 'o'>
    
    
    auto defaulted = int_types.as_defaulted_nontypes();         // nontype_list<0, 0>
```