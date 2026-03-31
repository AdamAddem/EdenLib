### Made in C++23

This is a collection of containers, functions, and utilities that i've created for one of many reasons:
- The standard library doesn't provide a way of doing what I wanted
  - releasing_vector, stack_vector, enumBetween
- The standard library has a limited or annoying implementation of what I wanted
  - owned_ptr
- Convenience
  - assume_assert, typedefs, lifetime_observer
- I wanted to see if I could implement it myself
  - bitset, type_traits, concepts

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
- bitset.hpp, memory.hpp, concepts.hpp, type_traits.hpp
  - My own implementations of random standard library stuff, not worth using.
- type_flags.hpp
  - Flags that can be used across my library for various purposes.
- typedefs.hpp
  - Shortened type names for common types.
---
    