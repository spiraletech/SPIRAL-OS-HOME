#pragma once

#include "home/ids.hpp"
#include "home/result.hpp"
#include "home/world_clock.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace home {

enum class TemporalLayer : std::uint8_t {
    Event = 0,
    Scene = 1
};

struct TemporalDomain final {
    TimeDomainId id{};
    std::string key{};
    WorldTime base_world_time{};
    WorldTime base_domain_time{};
    std::uint64_t rate_numerator{1};
    std::uint64_t rate_denominator{1};
    bool paused{false};
    TemporalLayer layer{TemporalLayer::Event};
    std::int32_t priority{};
};

struct ResolvedTemporalTime final {
    WorldTime canonical_time{};
    WorldTime effective_time{};
    std::optional<TimeDomainId> overlay{};
};

Result<TemporalDomain> make_temporal_domain(
    TimeDomainId id,
    std::string key,
    WorldTime base_world_time,
    WorldTime base_domain_time,
    std::uint64_t rate_numerator = 1,
    std::uint64_t rate_denominator = 1,
    bool paused = false,
    TemporalLayer layer = TemporalLayer::Event,
    std::int32_t priority = 0);

Result<WorldTime> project_domain_time(const TemporalDomain& domain, WorldTime world_time);
Result<TemporalDomain> rebase_temporal_domain(const TemporalDomain& domain, WorldTime world_time, bool paused);

class TemporalDomainRegistry final {
public:
    Result<void> add(TemporalDomain domain);
    Result<void> remove(TimeDomainId id);

    [[nodiscard]] const TemporalDomain* find(TimeDomainId id) const noexcept;
    [[nodiscard]] const TemporalDomain* find_by_key(const std::string& key) const noexcept;
    [[nodiscard]] const std::vector<TemporalDomain>& domains() const noexcept { return domains_; }

    Result<WorldTime> project(TimeDomainId id, WorldTime canonical_time) const;
    Result<ResolvedTemporalTime> resolve(WorldTime canonical_time) const;

private:
    std::vector<TemporalDomain> domains_{};
};

} // namespace home
