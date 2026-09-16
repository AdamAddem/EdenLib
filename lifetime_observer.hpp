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

    inline static constinit auto name_ = LifetimeObserver::name;
    sz_t id;
  public:

    LifetimeObserver() noexcept : id(idgen++)
    { lifetime_log.emplace_back(std::format("{} instance {}: Default Constructed\n", name_, id)); }

    LifetimeObserver(LifetimeObserver const& other) noexcept : id(idgen++)
    { lifetime_log.emplace_back(std::format("{} instance {}: Copy Constructed with id {}\n", name_, id, other.id)); }

    LifetimeObserver& operator=(LifetimeObserver const& other) noexcept
    { lifetime_log.emplace_back(std::format("{} id {}: Copy Assigned with {}\n", name_, id, other.id)); return *this; }

    LifetimeObserver(LifetimeObserver&& other) noexcept : id(idgen++)
    { lifetime_log.emplace_back(std::format("{} id {}: Move Constructed with {}\n", name_, id, other.id)); }

    LifetimeObserver& operator=(LifetimeObserver&& other) noexcept
    { lifetime_log.emplace_back(std::format("{} id {}: Move Assigned with {}\n", name_, id, other.id)); return *this; }

    ~LifetimeObserver()
    { lifetime_log.emplace_back(std::format("{} id {}: Destructed\n", name_, id)); }

    edenInlineNodiscard static auto const& getLog() noexcept { return lifetime_log; }
    edenInlineNodiscard sz_t getId() const noexcept { return id; }
  };


}