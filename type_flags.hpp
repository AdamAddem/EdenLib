#pragma once

namespace eden {

namespace flags {

struct FlagBase {
protected:
  consteval FlagBase() noexcept = default;
};

struct Exclusive : FlagBase {Exclusive() = delete;};
struct Inclusive : FlagBase {Inclusive() = delete;};

struct Less : FlagBase {Less() = delete;};
struct LessThan : FlagBase {LessThan() = delete;};
struct Greater : FlagBase {Greater() = delete;};
struct GreaterThan : FlagBase {GreaterThan() = delete;};

struct DoNotInitialize : FlagBase { consteval DoNotInitialize() noexcept = default; };
inline constexpr DoNotInitialize do_not_initialize;

struct End : FlagBase { consteval End() noexcept = default; };
inline constexpr End end;

template <sz_t N>
requires (N > 0)
struct ReserveInitial : FlagBase { consteval ReserveInitial() noexcept = default; };

template <sz_t N>
inline constexpr ReserveInitial<N> reserve_initial;
}

template <class T>
concept EdenFlag = std::derived_from<T, flags::FlagBase>;

template <class T>
concept ExclusivityFlag = std::is_same_v<T,flags::Exclusive> or std::is_same_v<T, flags::Inclusive>;

template <class T>
concept OrderingFlag =  std::is_same_v<T, flags::Less> or std::is_same_v<T, flags::LessThan> or
                        std::is_same_v<T, flags::Greater> or std::is_same_v<T, flags::GreaterThan>;

}