#pragma once
#include "concepts.hpp"

namespace eden {

namespace flags {
struct Exclusive : type<Exclusive> {Exclusive() = delete;};
struct Inclusive : type<Inclusive> {Inclusive() = delete;};
}

template <class T>
concept ExclusivityFlag = flags::Exclusive::is<T> or flags::Inclusive::is<T>;


}