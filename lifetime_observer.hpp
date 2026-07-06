#pragma once
#include "metaprogramming/type_class.hpp"
#include "typedefs.hpp"

#include <format>
#include <string>
#include <vector>

namespace eden {

  template<sz_t N>
  class LifetimeObserver : type<LifetimeObserver<N>, append_number_to_literal<N, "LifetimeObserver">> {
    inline static constinit std::vector<std::string> lifetime_log;
    inline static constinit sz_t idgen{1};
    inline static constinit const char* name_ = LifetimeObserver::name;
    sz_t id;
  public:

    constexpr LifetimeObserver() : id(idgen++)
    { lifetime_log.emplace_back(std::format("{} instance {}: Default Constructed\n", name_, id)); }

    constexpr LifetimeObserver(LifetimeObserver const& other) noexcept : id(idgen++)
    { lifetime_log.emplace_back(std::format("{} instance {}: Copy Constructed with id {}\n", name_, id, other.id)); }

    constexpr LifetimeObserver& operator=(LifetimeObserver const& other) noexcept
    { lifetime_log.emplace_back(std::format("{} id {}: Copy Assigned with {}\n", name_, id, other.id)); return *this; }

    constexpr LifetimeObserver(LifetimeObserver&& other) noexcept : id(idgen++)
    { lifetime_log.emplace_back(std::format("{} id {}: Move Constructed with {}\n", name_, id, other.id)); }

    constexpr LifetimeObserver& operator=(LifetimeObserver&& other) noexcept
    { lifetime_log.emplace_back(std::format("{} id {}: Move Assigned with {}\n", name_, id, other.id)); return *this; }

    constexpr ~LifetimeObserver()
    { lifetime_log.emplace_back(std::format("{} id {}: Destructed\n", name_, id)); }

    eden_always_inline [[nodiscard]] static constexpr auto const& getLog() noexcept { return lifetime_log; }
    eden_always_inline [[nodiscard]] constexpr sz_t getId() const noexcept { return id; }
  };


}