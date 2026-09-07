#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"
#include "home/world_clock.hpp"

#include <compare>
#include <cstdint>
#include <string>

namespace home {

struct TemporalDomain final {
    TimeDomainId id{};
    std::string key{};
    WorldTime base_world_time{};
    WorldTime base_domain_time{};
    std::uint64_t rate_numerator{1};
    std::uint64_t rate_denominator{1};
    bool paused{false};
};

Result<TemporalDomain> make_temporal_domain(
    TimeDomainId id,
    std::string key,
    WorldTime base_world_time,
    WorldTime base_domain_time,
    std::uint64_t rate_numerator = 1,
    std::uint64_t rate_denominator = 1,
    bool paused = false);

Result<WorldTime> project_domain_time(const TemporalDomain& domain, WorldTime world_time);
Result<TemporalDomain> rebase_temporal_domain(const TemporalDomain& domain, WorldTime world_time, bool paused);

} // namespace home
