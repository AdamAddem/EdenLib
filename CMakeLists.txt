cmake_minimum_required(VERSION 3.31)
project(Eden)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Werror -Wno-error=deprecated-declarations -Wpedantic -O0")

include_directories(${PROJECT_SOURCE_DIR}/src)
add_executable(Eden
        tests.cpp
        arena.hpp
        assume_assert.hpp
        concepts.hpp
        memory.hpp
        null_conditional_chaining.hpp
        owned.hpp
        releasing_vector.hpp
        stack_vector.hpp
        type_flags.hpp
        typedefs.hpp
        eden_all.hpp
        enum_utils.hpp
        lifetime_observer.hpp
)