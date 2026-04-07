#pragma once
#include <string>
#include "typedefs.hpp"

#include <format>

namespace eden {

  template<sz_t N>
  struct LifetimeObserver {
    inline static constinit std::string lifetime_log;
    inline static constinit sz_t idgen{0};

    LifetimeObserver() : id(idgen++)
    {lifetime_log.append(std::format("{} Default Constructed\n", id));}

    ~LifetimeObserver()
    {lifetime_log.append(std::format("{} Destructed\n", id));}

    LifetimeObserver(const LifetimeObserver& other) : id(idgen++)
    {lifetime_log.append(std::format("{} Copy Constructed with {}\n", id, other.id));}

    LifetimeObserver& operator=(const LifetimeObserver& other)
    {lifetime_log.append(std::format("{} Copy Assigned with {}\n", id, other.id)); return *this;}

    LifetimeObserver(LifetimeObserver&& other) noexcept : id(idgen++)
    {lifetime_log.append(std::format("{} Move Constructed with {}\n", id, other.id));}

    LifetimeObserver& operator=(LifetimeObserver&& other) noexcept
    {lifetime_log.append(std::format("{} Move Assigned with {}\n", id, other.id)); return *this;}

  private:
    sz_t id;
    static constexpr sz_t n = N;
  };


}